/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DTM
#include "be/dtm0_log.h"
#include "be/op.h"		/* M0_BE_OP_SYNC */
#include "lib/assert.h"		/* M0_PRE */
#include "lib/errno.h"		/* ENOENT */
#include "lib/memory.h"		/* M0_ALLOC */
#include "lib/trace.h"

enum {
	M0_BE_DTM_LREC_MAGIX = 0xdeaddeaddeaddead,
	M0_BE_DTM_LOG_MAGIX  = 0xbadbadbadbadbadb
};

/* Volatile list */
M0_TL_DESCR_DEFINE(vlrec, "DTM0 Volatile Log", static, struct m0_dtm0_log_rec,
                   dlr_tlink, dlr_magic, M0_BE_DTM_LREC_MAGIX,
                   M0_BE_DTM_LOG_MAGIX);

M0_TL_DEFINE(vlrec, static, struct m0_dtm0_log_rec);

/* Persistent list */
M0_BE_LIST_DESCR_DEFINE(plrec, "DTM0 Persistent Log", static,
                        struct m0_dtm0_log_rec, dlr_link, dlr_magic,
                        M0_BE_DTM_LREC_MAGIX, M0_BE_DTM_LOG_MAGIX);

M0_BE_LIST_DEFINE(plrec, static, struct m0_dtm0_log_rec);

static bool m0_be_dtm0_log__invariant(const struct m0_be_dtm0_log *log)
{
	return _0C(log != NULL) &&
	       _0C(log->dl_cs != NULL) &&
	       (log->dl_ispstore ?: _0C(vlrec_tlist_invariant(log->dl_tlist)));
}

static bool m0_dtm0_log_rec__invariant(const struct m0_dtm0_log_rec *rec,
                                       bool                          ispstore)
{
	return _0C(rec != NULL) &&
	       _0C(m0_dtm0_tx_desc__invariant(&rec->dlr_txd)) &&
	       (ispstore ?: _0C(m0_tlink_invariant(&vlrec_tl, rec)));
}

M0_INTERNAL int m0_be_dtm0_log_init(struct m0_be_dtm0_log **out,
                                    struct m0_dtm0_clk_src *cs,
                                    bool                    ispstore)
{
	struct m0_be_dtm0_log *log;

	M0_PRE(out != NULL);
	M0_PRE(cs != NULL);

	if (ispstore) {
		log = *out;
	} else {
		M0_ALLOC_PTR(log);
		if (log == NULL)
			return M0_ERR(-ENOMEM);

		M0_SET0(log);
		M0_ALLOC_PTR(log->dl_tlist);
		if (log->dl_tlist == NULL) {
			m0_free(log);
			return M0_ERR(-ENOMEM);
		}

		vlrec_tlist_init(log->dl_tlist);
		*out = log;
	}

	log->dl_ispstore = ispstore;
	m0_mutex_init(&log->dl_lock);
	log->dl_cs = cs;
	return 0;
}

M0_INTERNAL void m0_be_dtm0_log_fini(struct m0_be_dtm0_log **log)
{
	struct m0_be_dtm0_log *plog = *log;

	M0_PRE(m0_be_dtm0_log__invariant(plog));
	m0_mutex_fini(&plog->dl_lock);
	plog->dl_cs = NULL;

	if (!plog->dl_ispstore) {
		vlrec_tlist_fini(plog->dl_tlist);
		m0_free(plog->dl_tlist);
		m0_free(plog);
		*log = NULL;
	}
}

static void m0_be_dtm0_log_credit__prune(struct m0_be_dtm0_log    *log,
                                         const struct m0_dtm0_tid *id,
                                         struct m0_be_tx_credit   *accum)
{
	/* This assignment is meaningful as it covers the empty log case */
	int                     rc = M0_DTS_LT;
	struct m0_be_tx_credit  cred = {};
	struct m0_dtm0_log_rec *rec;

	m0_be_list_for (plrec, log->dl_list, rec) {
		plrec_be_list_credit(M0_BLO_TLINK_DESTROY, 1, &cred);
		plrec_be_list_credit(M0_BLO_DEL, 1, &cred);

		rc = m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id,
                                     id);

		if (rc == M0_DTS_EQ)
			break;
	} m0_be_list_endfor;

	if (rc == M0_DTS_EQ)
		m0_be_tx_credit_add(accum, &cred);
}

M0_INTERNAL void m0_be_dtm0_log_credit(enum m0_be_dtm0_log_credit_op op,
                                       struct m0_be_dtm0_log        *log,
                                       struct m0_be_tx              *tx,
                                       struct m0_be_seg             *seg,
                                       const struct m0_dtm0_tid      *id,
                                       struct m0_be_tx_credit       *accum,
                                       uint32_t                      nr_pa,
                                       uint64_t                      size)
{
	struct m0_be_tx_credit cred_pa;
	struct m0_be_tx_credit cred_lrec;
	struct m0_be_tx_credit cred_log;
	struct m0_be_tx_credit cred_buf;

	cred_buf  = M0_BE_TX_CREDIT(1, size);
	cred_log  = M0_BE_TX_CREDIT_TYPE(struct m0_be_dtm0_log);
	cred_lrec = M0_BE_TX_CREDIT_TYPE(struct m0_dtm0_log_rec);
	cred_pa   = M0_BE_TX_CREDIT_TYPE(struct m0_dtm0_tx_pa);
	m0_be_tx_credit_mul(&cred_pa, nr_pa);

	switch (op) {
	case M0_DTML_CREATE:
		m0_be_tx_credit_add(accum, &cred_log);
		plrec_be_list_credit(M0_BLO_CREATE, 1, accum);
		break;
	case M0_DTML_DESTROY:
		plrec_be_list_credit(M0_BLO_DESTROY, 1, accum);
		break;
	case M0_DTML_PERSISTENT:
		plrec_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
		plrec_be_list_credit(M0_BLO_ADD, 1, accum);
		m0_be_tx_credit_add(accum, &cred_lrec);
		m0_be_tx_credit_add(accum, &cred_pa);
		break;
	case M0_DTML_SENT:
	case M0_DTML_EXECUTED:
	case M0_DTML_REDO:
		plrec_be_list_credit(M0_BLO_TLINK_CREATE, 1, accum);
		plrec_be_list_credit(M0_BLO_ADD, 1, accum);
		m0_be_tx_credit_add(accum, &cred_lrec);
		m0_be_tx_credit_add(accum, &cred_pa);
		m0_be_tx_credit_add(accum, &cred_buf);
		break;
	case M0_DTML_PRUNE:
		m0_be_dtm0_log_credit__prune(log, id, accum);
		break;
	default:
		M0_IMPOSSIBLE("");
	}
}

M0_INTERNAL int m0_be_dtm0_log_create(struct m0_be_tx        *tx,
                                      struct m0_be_seg       *seg,
                                      struct m0_be_dtm0_log **out)
{
	struct m0_be_dtm0_log *log;

	M0_PRE(tx != NULL);
	M0_PRE(seg != NULL);
	M0_PRE(m0_be_tx__invariant(tx));

	M0_BE_ALLOC_PTR_SYNC(log, seg, tx);
	if (log == NULL)
		return M0_ERR(-ENOMEM);

	M0_BE_ALLOC_PTR_SYNC(log->dl_list, seg, tx);
	if (log->dl_list == NULL) {
		M0_BE_FREE_PTR_SYNC(log, seg, tx);
		return M0_ERR(-ENOMEM);
	}

	M0_BE_ALLOC_PTR_SYNC(log->dl_dlist, seg, tx);
	if (log->dl_dlist == NULL) {
		M0_BE_FREE_PTR_SYNC(log, seg, tx);
		M0_BE_FREE_PTR_SYNC(log->dl_list, seg, tx);
		return M0_ERR(-ENOMEM);
	}

	plrec_be_list_create(log->dl_list, tx);
	plrec_be_list_create(log->dl_dlist, tx);
	M0_BE_TX_CAPTURE_PTR(seg, tx, log);
	return 0;
}

M0_INTERNAL void m0_be_dtm0_log_destroy(struct m0_be_tx        *tx,
					struct m0_be_seg       *seg,
                                        struct m0_be_dtm0_log **log)
{
	struct m0_be_dtm0_log *plog = *log;

	M0_PRE(tx != NULL);
	M0_PRE(seg != NULL);
	M0_PRE(plog != NULL);
	M0_PRE(plog->dl_ispstore);
	M0_PRE(m0_be_tx__invariant(tx));

	plrec_be_list_destroy(plog->dl_list, tx);
	plrec_be_list_destroy(plog->dl_dlist, tx);
	M0_BE_FREE_PTR_SYNC(plog->dl_list, seg, tx);
	M0_BE_FREE_PTR_SYNC(plog->dl_dlist, seg, tx);
	M0_BE_FREE_PTR_SYNC(plog, seg, tx);
	*log = NULL;
}

static struct m0_dtm0_log_rec *
m0_be_dtm0_log_find__plist(struct m0_be_dtm0_log    *log,
                           const struct m0_dtm0_tid *id)
{
	struct m0_dtm0_log_rec *rec;

	m0_be_list_for (plrec, log->dl_list, rec) {
		if (m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id,
				    id) == M0_DTS_EQ)
			return rec;
	} m0_be_list_endfor;

	return NULL;
}

static struct m0_dtm0_log_rec *
m0_be_dtm0_log_find__vlist(struct m0_be_dtm0_log    *log,
                           const struct m0_dtm0_tid *id)

{
	return m0_tl_find(vlrec, rec, log->dl_tlist,
			  m0_dtm0_tid_cmp(log->dl_cs,
					  &rec->dlr_txd.dtd_id,
					  id) == M0_DTS_EQ);
}

M0_INTERNAL
struct m0_dtm0_log_rec *m0_be_dtm0_log_find(struct m0_be_dtm0_log    *log,
                                            const struct m0_dtm0_tid *id)
{
	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	if (log->dl_ispstore)
		return m0_be_dtm0_log_find__plist(log, id);
	else
		return m0_be_dtm0_log_find__vlist(log, id);
}


static int m0_be_dtm0_log_rec_init__vlist(struct m0_dtm0_log_rec **rec,
                                          struct m0_be_dtm0_log   *log,
                                          struct m0_be_tx         *tx,
                                          struct m0_be_seg        *seg,
                                          struct m0_dtm0_tx_desc  *txd,
                                          struct m0_buf           *pyld)
{
	int                     rc;
	struct m0_dtm0_log_rec *lrec;

	M0_PRE(tx == NULL);
	M0_PRE(seg == NULL);

	M0_ALLOC_PTR(lrec);
	if (lrec == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_dtm0_tx_desc_copy(tx, seg, txd, &lrec->dlr_txd,
				  log->dl_ispstore);
	if (rc != 0) {
		m0_free(lrec);
		return rc;
	}

	rc = m0_buf_copy(&lrec->dlr_pyld, pyld);
	if (rc != 0) {
		m0_dtm0_tx_desc_fini(tx, seg, &lrec->dlr_txd,
				     log->dl_ispstore);
		m0_free(lrec);
		return rc;
	}

	*rec = lrec;
	return 0;
}

static int m0_be_dtm0_log_rec_init__plist(struct m0_dtm0_log_rec **rec,
                                          struct m0_be_dtm0_log   *log,
                                          struct m0_be_tx         *tx,
                                          struct m0_be_seg        *seg,
                                          struct m0_dtm0_tx_desc  *txd,
                                          struct m0_buf           *pyld)
{
	int                     rc;
	struct m0_dtm0_log_rec *lrec;

	M0_PRE(m0_be_tx__invariant(tx));
	M0_PRE(m0_be_seg__invariant(seg));

	M0_BE_ALLOC_PTR_SYNC(lrec, seg, tx);

	rc = m0_dtm0_tx_desc_copy(tx, seg, txd, &lrec->dlr_txd,
				  log->dl_ispstore);
	if (rc != 0) {
		M0_BE_FREE_PTR_SYNC(lrec, seg, tx);
		return rc;
	}

	if (pyld->b_nob) {
		lrec->dlr_pyld.b_nob = pyld->b_nob;
		M0_BE_ALLOC_BUF_SYNC(&lrec->dlr_pyld, seg, tx);
		memcpy(lrec->dlr_pyld.b_addr, pyld->b_addr,
		       pyld->b_nob);

		M0_BE_TX_CAPTURE_BUF(seg, tx, &lrec->dlr_pyld);
	}

	M0_BE_TX_CAPTURE_ARR(seg, tx, lrec->dlr_txd.dtd_pg.dtpg_pa,
			     lrec->dlr_txd.dtd_pg.dtpg_nr);
	M0_BE_TX_CAPTURE_PTR(seg, tx, lrec);

	*rec = lrec;
	return 0;
}

static int m0_be_dtm0_log_rec_init(struct m0_dtm0_log_rec **rec,
                                   struct m0_be_dtm0_log   *log,
                                   struct m0_be_tx         *tx,
                                   struct m0_be_seg        *seg,
                                   struct m0_dtm0_tx_desc  *txd,
                                   struct m0_buf           *pyld)
{
	if (log->dl_ispstore)
		return m0_be_dtm0_log_rec_init__plist(rec, log, tx, seg, txd,
                                                      pyld);
	else
		return m0_be_dtm0_log_rec_init__vlist(rec, log, tx, seg, txd,
                                                      pyld);
}

static void m0_be_dtm0_log_rec_fini(struct m0_be_dtm0_log   *log,
                                    struct m0_be_tx         *tx,
                                    struct m0_be_seg        *seg,
                                    struct m0_dtm0_log_rec **rec)
{
	struct m0_dtm0_log_rec *lrec = *rec;

	m0_dtm0_tx_desc_fini(tx, seg, &lrec->dlr_txd, log->dl_ispstore);
	if (log->dl_ispstore) {
		M0_BE_FREE_PTR_SYNC(lrec->dlr_pyld.b_addr, seg, tx);
		M0_BE_FREE_PTR_SYNC(lrec, seg, tx);
	} else {
		m0_buf_free(&lrec->dlr_pyld);
		m0_free(lrec);
	}

	*rec = NULL;
}

static int m0_be_dtm0_log__insert(struct m0_be_dtm0_log  *log,
                                  struct m0_be_tx        *tx,
                                  struct m0_be_seg       *seg,
                                  struct m0_dtm0_tx_desc *txd,
                                  struct m0_buf          *pyld)
{
	int                     rc;
	struct m0_dtm0_log_rec *rec;

	rc = m0_be_dtm0_log_rec_init(&rec, log, tx, seg, txd, pyld);
	if (rc !=  0)
		return rc;

	if (log->dl_ispstore) {
		plrec_be_tlink_create(rec, tx);
		plrec_be_list_add_tail(log->dl_list, tx, rec);
	} else {
		vlrec_tlink_init_at_tail(rec, log->dl_tlist);
	}

	return rc;
}


static int m0_be_dtm0_log__set(struct m0_be_dtm0_log  *log,
                               struct m0_be_tx        *tx,
                               struct m0_be_seg       *seg,
                               struct m0_dtm0_tx_desc *txd,
                               struct m0_buf          *pyld,
                               struct m0_dtm0_log_rec *rec)
{
	int                     rc;
	int                     pa_id;
	struct m0_dtm0_tx_desc *ltxd     = &rec->dlr_txd;
	struct m0_buf          *lpyld    = &rec->dlr_pyld;
	struct m0_dtm0_tx_pa   *ldtpg_pa = ltxd->dtd_pg.dtpg_pa;
	struct m0_dtm0_tx_pa   *dtpg_pa  = txd->dtd_pg.dtpg_pa;
	uint32_t                num_pa   = ltxd->dtd_pg.dtpg_nr;

	M0_PRE(m0_dtm0_log_rec__invariant(rec, log->dl_ispstore));

	/* Attach payload to log if it is not attached */
	if (!m0_dtm0_txr_rec_is_set(lpyld) && m0_dtm0_txr_rec_is_set(pyld)) {
		rc = m0_buf_copy(lpyld, pyld);
		if (rc != 0)
			return rc;

		if (log->dl_ispstore) {
			M0_BE_TX_CAPTURE_BUF(seg, tx, lpyld);
			M0_BE_TX_CAPTURE_PTR(seg, tx, rec);
		}
	}

	for (pa_id = 0; pa_id < num_pa; ++pa_id) {
		m0_dtm0_update_pa_state(&ldtpg_pa[pa_id].pa_state,
                                        &dtpg_pa[pa_id].pa_state);
	}

	if (log->dl_ispstore)
		M0_BE_TX_CAPTURE_ARR(seg, tx, ldtpg_pa, num_pa);

	return 0;
}

M0_INTERNAL int m0_be_dtm0_log_update(struct m0_be_dtm0_log  *log,
                                      struct m0_be_tx        *tx,
                                      struct m0_be_seg       *seg,
                                      struct m0_dtm0_tx_desc *txd,
                                      struct m0_buf          *pyld)
{
	struct m0_dtm0_log_rec *rec;

	M0_PRE(pyld != NULL);
	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tx_desc__invariant(txd));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	return (rec = m0_be_dtm0_log_find(log, &txd->dtd_id)) ?
                m0_be_dtm0_log__set(log, tx, seg, txd, pyld, rec) :
                m0_be_dtm0_log__insert(log, tx, seg, txd, pyld);
}

static int m0_be_dtm0_log_prune__vlist(struct m0_be_dtm0_log    *log,
                                       struct m0_be_tx          *tx,
                                       struct m0_be_seg         *seg,
                                       const struct m0_dtm0_tid *id)
{
	/* This assignment is meaningful as it covers the empty log case */
	int                     rc = M0_DTS_LT;
	struct m0_dtm0_log_rec *rec;
	struct m0_dtm0_log_rec *currec;

	m0_tl_for (vlrec, log->dl_tlist, rec) {
		if (!m0_dtm0_is_rec_is_stable(&rec->dlr_txd.dtd_pg))
			return M0_ERR(-EPROTO);

		rc = m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id,
				     id);
		if (rc != M0_DTS_LT)
			break;
	} m0_tl_endfor;

	if (rc != M0_DTS_EQ)
		return M0_ERR(-ENOENT);

	while ((currec = vlrec_tlist_pop(log->dl_tlist)) != rec) {
		M0_ASSERT(m0_dtm0_log_rec__invariant(currec, log->dl_ispstore));
		m0_be_dtm0_log_rec_fini(log, tx, seg, &currec);
	}

	m0_be_dtm0_log_rec_fini(log, tx, seg, &currec);
	return rc;
}

static int m0_be_dtm0_log_prune__plist(struct m0_be_dtm0_log    *log,
                                       struct m0_be_tx          *tx,
                                       struct m0_be_seg         *seg,
                                       const struct m0_dtm0_tid *id)
{
	/* This assignment is meaningful as it covers the empty log case */
	int                     rc = M0_DTS_LT;
	struct m0_dtm0_log_rec *rec;
	struct m0_dtm0_log_rec *currec;

	m0_be_list_for (plrec, log->dl_list, rec) {
		if (!m0_dtm0_is_rec_is_stable(&rec->dlr_txd.dtd_pg))
			return M0_ERR(-EPROTO);

		rc = m0_dtm0_tid_cmp(log->dl_cs, &rec->dlr_txd.dtd_id,
                                     id);

		if (rc != M0_DTS_LT)
			break;
	} m0_be_list_endfor;

	if (rc != M0_DTS_EQ)
		return M0_ERR(-ENOENT);

	while ((currec = plrec_be_list_head(log->dl_list))) {
		if (currec != rec) {
			plrec_be_list_del(log->dl_list, tx, currec);
			plrec_be_tlink_destroy(currec, tx);
			m0_be_dtm0_log_rec_fini(log, tx, seg, &currec);
		}
	}

	M0_ASSERT(currec == rec);
	plrec_be_list_del(log->dl_list, tx, rec);
	plrec_be_tlink_destroy(rec, tx);
	m0_be_dtm0_log_rec_fini(log, tx, seg, &rec);
	return 0;
}

M0_INTERNAL int m0_be_dtm0_log_prune(struct m0_be_dtm0_log    *log,
                                     struct m0_be_tx          *tx,
                                     struct m0_be_seg         *seg,
                                     const struct m0_dtm0_tid *id)
{
	M0_PRE(m0_be_dtm0_log__invariant(log));
	M0_PRE(m0_dtm0_tid__invariant(id));
	M0_PRE(m0_mutex_is_locked(&log->dl_lock));

	if (log->dl_ispstore)
		return m0_be_dtm0_log_prune__plist(log, tx, seg, id);
	else
		return m0_be_dtm0_log_prune__vlist(log, tx, seg, id);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
