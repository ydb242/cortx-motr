/* -*- C -*- */
/*
 * Copyright (c) 2013-2021 Seagate Technology LLC and/or its Affiliates
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

#pragma once

#ifndef __MOTR_BTREE_INTERNAL_H__
#define __MOTR_BTREE_INTERNAL_H__

#include "sm/op.h"
#include "be/op.h"   /* m0_be_op */

/**
 * @defgroup btree
 *
 * @{
 */

enum m0_btree_opcode;
struct m0_btree_oimpl;

struct stats_gnf {
	uint64_t bn_key_durr;
	uint64_t bn_key_count;
	uint64_t ext_keycmp_durr;
	uint64_t ext_keycmp_count;
	uint64_t int_keycmp_durr;
	uint64_t int_keycmp_count;
	uint64_t inside_durr;
	uint64_t inside_count;
};

struct stats_gng {
	uint64_t seg_isvalid_true_durr;
	uint64_t seg_isvalid_true_count;
	uint64_t opq_durr;
	uint64_t opq_count;
	uint64_t list_lock_durr;
	uint64_t list_lock_count;
	uint64_t block1_durr;
	uint64_t block1_count;
	uint64_t block2_durr;
	uint64_t block2_count;
	uint64_t alloc_durr;
	uint64_t alloc_count;
	uint64_t tlink_durr;
	uint64_t tlink_count;
	uint64_t crc_durr;
	uint64_t crc_count;
	uint64_t inside_durr;
	uint64_t inside_count;
	uint64_t lru_durr;
	uint64_t lru_count;

};

struct stats_put_data {
	uint64_t INIT_dur;
	uint64_t SETUP_dur;
	uint64_t DOWN_dur;
	uint64_t NEXTDOWN_dur;
	uint64_t NEXTDOWN_iter_count;
	uint64_t ALLOC_REQUIRE_dur;
	uint64_t ALLOC_REQUIRE_iter_count;
	uint64_t ALLOC_STORE_dur;
	uint64_t LOCK_dur;
	uint64_t CHECK_dur;
	uint64_t SANITY_dur;
	uint64_t MAKESPACE_dur;
	uint64_t ACT_dur;
	uint64_t CAPTURE_dur;
	uint64_t CLEANUP_dur;
};

struct stats_get_data {
	uint64_t INIT_dur;
	uint64_t SETUP_dur;
	uint64_t DOWN_dur;
	uint64_t NEXTDOWN_dur;
	uint64_t NEXTDOWN_iter_count;
	uint64_t LOCK_dur;
	uint64_t CHECK_dur;
	uint64_t ACT_dur;
	uint64_t cb_dur;
	uint64_t cb_count;
	uint64_t CLEANUP_dur;
//NEXTDOWN
	uint64_t block_dur;
	uint64_t block_count;
	uint64_t bisvalid_dur;
	uint64_t bisvalid_count;
	uint64_t bfind_dur;
	uint64_t bfind_count;
	uint64_t bget_dur;
	uint64_t bget_count;
	struct stats_gnf nxt_gnf;
	struct stats_gng nxt_gng;
};

struct stats_del_data {
	uint64_t INIT_dur;
	uint64_t SETUP_dur;
	uint64_t DOWN_dur;
	uint64_t NEXTDOWN_dur;
	uint64_t NEXTDOWN_iter_count;
	uint64_t STORE_CHILD_dur;
	uint64_t LOCK_dur;
	uint64_t CHECK_dur;
	uint64_t ACT_dur;
	uint64_t CAPTURE_dur;
	uint64_t FREENODE_dur;
	uint64_t FREENODE_iter_count;
	uint64_t CLEANUP_dur;
};

struct stats_nextdown_phase {
	uint64_t block_dur;
	uint64_t bfind_dur;
	uint64_t bget_dur;
	uint64_t block_count;
	uint64_t bfind_count;
	uint64_t bget_count;
	uint64_t block_avg;
	uint64_t bfind_avg;
	uint64_t bget_avg;
	uint64_t bisvalid_dur;
	uint64_t bisvalid_count;
	uint64_t bisvalid_avg;
	uint64_t num_sample;
};
struct stats_act_phase {
	uint64_t cb_dur;
	uint64_t cb_count;
	uint64_t num_sample;
};
union stats_ext_data {
	struct stats_put_data put_stats;
	struct stats_get_data get_stats;
	struct stats_del_data del_stats;
};


struct m0_btree_op {
	struct m0_sm_op             bo_op;
	struct m0_sm_group          bo_sm_group;
	struct m0_sm_op_exec        bo_op_exec;
	enum m0_btree_opcode        bo_opc;
	struct m0_btree            *bo_arbor;
	struct m0_btree_rec         bo_rec;
	struct m0_btree_cb          bo_cb;
	struct m0_be_tx            *bo_tx;
	struct m0_be_seg           *bo_seg;
	uint64_t                    bo_flags;
	m0_bcount_t                 bo_limit;
	struct m0_btree_oimpl      *bo_i;
	struct m0_btree_idata       bo_data;
	struct m0_btree_rec_key_op  bo_keycmp;

	void                       *all_data;
};

struct btree_stats {
	uint64_t min_dur;
	uint64_t max_dur;
	uint64_t avg_dur;
	uint64_t tot_dur;
	uint64_t sample_count;

	uint64_t nxt_dwn_duration;
	uint64_t nxt_dwn_count;
	uint64_t act_duration;
	uint64_t init_duration;
	uint64_t setup_duration;
	uint64_t down_duration;
	uint64_t cleanup_duration;

	uint64_t nxt_dwn_avg;
	uint64_t act_avg;
	uint64_t init_avg;
	uint64_t setup_avg;
	uint64_t down_avg;
	uint64_t cleanup_avg;

	union stats_ext_data min_data;

	union stats_ext_data max_data;
};

#define RECORD_START_TIME(startTime) startTime = m0_time_now()
#define RECORD_END_TIME(endTime)     endTime = m0_time_now()

#define UPDATE_STATS(stats, startTime,  endTime, details)                      \
		{                                                              \
			uint64_t duration = endTime - startTime;               \
			if (stats.min_dur > duration) {                        \
				stats.min_dur = duration;                      \
				stats.min_data = details;                      \
			}                                                      \
			if (stats.max_dur < duration) {                        \
				stats.max_dur = duration;                      \
				stats.max_data = details;                      \
			}                                                      \
			stats.sample_count++;                                  \
			stats.tot_dur += duration;                             \
			stats.avg_dur = stats.tot_dur / stats.sample_count;    \
			stats.nxt_dwn_avg = stats.nxt_dwn_duration / stats.sample_count;    \
			stats.act_avg = stats.act_duration / stats.sample_count;    \
			stats.init_avg = stats.init_duration / stats.sample_count;    \
			stats.setup_avg = stats.setup_duration / stats.sample_count;    \
			stats.down_avg = stats.down_duration / stats.sample_count;    \
			stats.cleanup_avg = stats.cleanup_duration / stats.sample_count;    \
		}
#define UPDATE_TOTAL_GNF(total,single)\
{\
	total.bn_key_durr += single.bn_key_durr; \
	total.bn_key_count += single.bn_key_count ; \
	total.ext_keycmp_durr += single.ext_keycmp_durr ; \
	total.ext_keycmp_count += single.ext_keycmp_count ; \
	total.int_keycmp_durr += single.int_keycmp_durr ; \
	total.int_keycmp_count += single.int_keycmp_count ; \
	total.inside_durr += single.inside_durr ; \
	total.inside_count += single.inside_count ; \
}

#define UPDATE_TOTAL_GNG(total,single)\
{\
	total.seg_isvalid_true_durr += single.seg_isvalid_true_durr ;\
	total.seg_isvalid_true_count += single.seg_isvalid_true_count ;\
	total.opq_durr += single.opq_durr ;\
	total.opq_count += single.opq_count ;\
	total.list_lock_durr += single.list_lock_durr ;\
	total.list_lock_count += single.list_lock_count ;\
	total.block1_durr += single.block1_durr ;\
	total.block1_count += single.block1_count ;\
	total.block2_durr += single.block2_durr ;\
	total.block2_count += single.block2_count ;\
	total.alloc_durr += single.alloc_durr ;\
	total.alloc_count += single.alloc_count ;\
	total.tlink_durr += single.tlink_durr ;\
	total.tlink_count += single.tlink_count ;\
	total.crc_durr += single.crc_durr ;\
	total.crc_count += single.crc_count ;\
	total.inside_durr += single.inside_durr ;\
	total.inside_count += single.inside_count ;\
	total.lru_durr += single.lru_durr ;\
	total.lru_count += single.lru_count ;\
}

#define UPDATE_AVERAGE(stat)                                             \
	{							 \
		stat.block_avg = stat.block_dur/stat.num_sample; \
		stat.bfind_avg = stat.bfind_dur/stat.num_sample; \
		stat.bget_avg = stat.bget_dur/stat.num_sample;    \
		stat.bisvalid_avg = stat.bisvalid_dur/stat.num_sample;    \
	}
#ifndef __KERNEL__
extern struct btree_stats get_records;
extern struct btree_stats put_records;
extern struct btree_stats del_records;
#endif

enum m0_btree_node_format_version {
	M0_BTREE_NODE_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_BE_BNODE_FORMAT_VERSION */
	/*M0_BTREE_NODE_FORMAT_VERSION_2,*/
	/*M0_BTREE_NODE_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_BTREE_NODE_FORMAT_VERSION = M0_BTREE_NODE_FORMAT_VERSION_1
};

struct m0_btree_cursor {
	struct m0_buf    bc_key;
	struct m0_buf    bc_val;
	struct m0_btree *bc_arbor;
	struct m0_be_op  bc_op;
};

struct td;
struct m0_btree {
	const struct m0_btree_type *t_type;
	unsigned                    t_height;
	struct td                  *t_desc;
};

/** @} end of btree group */
#endif /* __MOTR_BTREE_INTERNAL_H__ */

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
