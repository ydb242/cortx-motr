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

/**
 * @addtogroup dtm0
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "dtm0/drlink.h"

#include "lib/time.h"           /* m0_nanosleep */
#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "rpc/rpclib.h"         /* m0_rpc_client_start */
#include "fid/fid.h"            /* M0_FID_INIT */
#include "net/net.h"            /* m0_net_xprt_default_get */
#include "be/queue.h"           /* m0_be_queue_init */

#include "dtm0/service.h"       /* m0_dtm0_service */
#include "dtm0/fop.h"           /* dtm0_req_fop */


enum {
	DTM0_UT_DRLINK_SIMPLE_POST_NR = 0x100,
};


/* XXX remove after transition to libfabric */
#ifndef M0_NET_XPRT_PREFIX_DEFAULT
#define M0_NET_XPRT_PREFIX_DEFAULT "lnet"
#endif
/* XXX copy-paste */
#define SERVER_ENDPOINT_ADDR   "0@lo:12345:34:1"
#define SERVER_ENDPOINT        M0_NET_XPRT_PREFIX_DEFAULT":"SERVER_ENDPOINT_ADDR
#define DTM0_UT_CONF_PROCESS   "<0x7200000000000001:5>"
#define DTM0_UT_LOG            "dtm0_ut_server.log"

// struct m0_reqh  *dtm0_cli_srv_reqh;

static struct m0_fid cli_srv_fid  = M0_FID_INIT(0x7300000000000001, 0x1a);
static struct m0_fid srv_dtm0_fid = M0_FID_INIT(0x7300000000000001, 0x1c);
static const char *cl_ep_addr  = "0@lo:12345:34:2";
static const char *srv_ep_addr = SERVER_ENDPOINT_ADDR;
static char *dtm0_ut_argv[] = { "m0d", "-T", "linux",
			       "-D", "dtm0_sdb", "-S", "dtm0_stob",
			       "-A", "linuxstob:dtm0_addb_stob",
			       "-e", SERVER_ENDPOINT,
			       "-H", SERVER_ENDPOINT_ADDR,
			       "-w", "10",
			       "-f", DTM0_UT_CONF_PROCESS,
			       "-c", M0_SRC_PATH("dtm0/conf.xc")};

/* XXX */
struct cl_ctx {
	struct m0_net_domain     cl_ndom;
	struct m0_rpc_client_ctx cl_ctx;
};

/* XXX */
void dtm0_ut_client_init(struct cl_ctx *cctx, const char *cl_ep_addr,
				const char *srv_ep_addr,
				struct m0_net_xprt *xprt);
/* XXX */
void dtm0_ut_client_fini(struct cl_ctx *cctx);

void m0_dtm0_ut_drlink_simple()
{
	// m0_time_t rem;
	int rc;
	struct cl_ctx            cctx = {};
	struct m0_rpc_server_ctx sctx = {
		.rsx_xprts         = m0_net_all_xprt_get(),
		.rsx_xprts_nr      = m0_net_xprt_nr(),
		.rsx_argv          = dtm0_ut_argv,
		.rsx_argc          = ARRAY_SIZE(dtm0_ut_argv),
		.rsx_log_file_name = DTM0_UT_LOG,
	};
	struct m0_reqh_service  *cli_srv;
	struct m0_reqh_service  *srv_srv;
	struct m0_dtm0_service  *svc;
	struct m0_dtm0_service  *svc_server;
	struct m0_reqh          *srv_reqh;
	struct m0_fom            fom = {};  // XXX just a fom
	struct m0_dtm0_tx_pa     pa = {};
	struct dtm0_req_fop *fop;
	struct m0_be_op     *op;
	struct m0_be_op      op_out = {};
	struct m0_fid       *fid;
	struct m0_fid        fid_out;
	bool                 successful;
	bool                 found;
	int                  i;
	int                  j;

	M0_ALLOC_ARR(fid, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(fid != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		fid[i] = M0_FID_INIT(0, i+1);  /* TODO set fid type */
	}
	M0_ALLOC_ARR(fop, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(fop != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		fop[i] = (struct dtm0_req_fop){
			.dtr_msg = DTM_TEST,
			.dtr_txr = {
				.dtd_id = {
					.dti_fid = fid[i],
				},
				.dtd_ps = {
					.dtp_nr = 1,
					.dtp_pa = &pa,
				},
			},
		};
	}
	M0_ALLOC_ARR(op, DTM0_UT_DRLINK_SIMPLE_POST_NR);
	M0_UT_ASSERT(op != NULL);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i)
		m0_be_op_init(&op[i]);

	srv_reqh = &sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh;

	// m0_fi_enable("m0_dtm0_in_ut", "ut");

	rc = m0_rpc_server_start(&sctx);
	M0_UT_ASSERT(rc == 0);

	dtm0_ut_client_init(&cctx, cl_ep_addr, srv_ep_addr,
			    m0_net_xprt_default_get());
	rc = m0_dtm_client_service_start(&cctx.cl_ctx.rcx_reqh,
					 &cli_srv_fid, &cli_srv);
	M0_UT_ASSERT(rc == 0);

	srv_srv = m0_reqh_service_lookup(srv_reqh, &srv_dtm0_fid);
	/* TODO export the function which does bob_of() */
	svc_server = container_of(srv_srv, struct m0_dtm0_service,
	                          dos_generic);

	svc = container_of(cli_srv, struct m0_dtm0_service, dos_generic);

	M0_ALLOC_PTR(svc->dos_ut_queue);
	M0_UT_ASSERT(svc->dos_ut_queue != 0);
	rc = m0_be_queue_init(svc->dos_ut_queue,
			      &(struct m0_be_queue_cfg){
			.bqc_q_size_max = DTM0_UT_DRLINK_SIMPLE_POST_NR,
			.bqc_producers_nr_max = DTM0_UT_DRLINK_SIMPLE_POST_NR,
			.bqc_consumers_nr_max = 1,
			.bqc_item_length = sizeof fid[0],
                              });
	M0_UT_ASSERT(rc == 0);

	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		rc = m0_dtm0_req_post(svc_server, &op[i], &fop[i],
		                      &cli_srv_fid, &fom, true);
		M0_UT_ASSERT(rc == 0);
	}
	m0_be_op_init(&op_out);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		successful = false;
		m0_be_queue_lock(svc->dos_ut_queue);
		M0_BE_QUEUE_GET(svc->dos_ut_queue, &op_out, &fid_out,
				&successful);
		m0_be_queue_unlock(svc->dos_ut_queue);
		m0_be_op_wait(&op_out);
		M0_UT_ASSERT(successful);
		m0_be_op_reset(&op_out);
		found = false;
		for (j = 0; j < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++j) {
			if (m0_fid_eq(&fid_out, &fid[j])) {
				found = true;
				fid[j] = M0_FID0;
				break;
			}
		}
		M0_UT_ASSERT(found);
	}
	m0_be_op_fini(&op_out);
	m0_be_queue_fini(svc->dos_ut_queue);
	m0_free(svc->dos_ut_queue);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i)
		M0_UT_ASSERT(m0_fid_eq(&fid[i], &M0_FID0));
	m0_free(fid);
	for (i = 0; i < DTM0_UT_DRLINK_SIMPLE_POST_NR; ++i) {
		m0_be_op_wait(&op[i]);
		m0_be_op_fini(&op[i]);
	}
	m0_free(op);
	m0_free(fop);

	// rem = M0_TIME_ONE_SECOND;
	// while (rem != 0)
		// m0_nanosleep(rem, &rem);  /* XXX dirty hack */


	m0_dtm_client_service_stop(cli_srv);
	dtm0_ut_client_fini(&cctx);

	m0_rpc_server_stop(&sctx);
	// m0_fi_disable("m0_dtm0_in_ut", "ut");

}


#undef M0_TRACE_SUBSYSTEM

/** @} end of dtm0 group */

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
