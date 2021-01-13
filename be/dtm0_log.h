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

#pragma once
#ifndef __MOTR_BE_DTM0_LOG_H__
#define __MOTR_BE_DTM0_LOG_H__

/**
 *  @page dtm0 log implementation
 *
 *  @section Overview
 *  DTM0 log module will be working on incoming message request, the goal of
 *  this module is to track journaling of incoming message either on persistent
 *  or volatile memory based on whether logging is happening on participant or
 *  on originator, so that in the phase failure consistency of failed
 *  participant can be restored by iterating over logged journal(DTM0 log
 *  record) to decide which of the logged record needs to be sent as a redo
 *  request.
 *
 *  A distributed transaction(dtx) has a group of states of each individual
 *  participant of the distributed transaction(dtx) (note: originator is not a
 *  part of this group). Each entry of such a group may have many different
 *  states, but this set of states must include the following states:
 *
 *	1) InProgress - the node (owner of the log) has a request.
 *	2) Executed - one or more operations have been executed in volatile
 *	              memory of the participant, the result of such execution
 *	              is known and returned to the dtx user.
 *	3) Persistent - locally persisted in transactional context.
 *	4) Stable - sufficient number of sent operations have been “persisted”
 *	            on the remote end which guarantees survivial of persistent
 *	            failures.
 *	5) DONE -  all sent operations have been “persisted” on sufficient
 *	           number of non-failed participants.
 *
 *  Every participant maintains the journal record(DTM0 Log record) that
 *  corresponds to each distributed transaction(dtx) and it can be described by
 *  "struct m0_dtm0_log_record" which will be stored in volatile/persistent
 *  storage. Basically this log record will maintain txr id, group of state of
 *  each individual participant and payload.
 *
 *  When a distributed transaction(dtx) is executed by participant, participant
 *  modify its state in group of state as M0_DTML_STATE_EXECUTED and will
 *  add/modify DTM0 log record, for rest of the participant it will keep the
 *  state as it is in distributed transaction(dtx).
 *
 *  When a distributed transaction(dtx) become persistent on participant,
 *  participant will modify its state in group of state as
 *  M0_DTML_STATE_PERSISTENT and will modify DTM0 log record, for rest of the
 *  participant it will keep the state as it is in DTM0 log record.
 *
 *  Originator also maintains the same distributed transaction(dtx) record in
 *  volatile memory. Originator is expecting to get replies from participant
 *  when distributed transaction(dtx) on participant is M0_DTML_STATE_EXECUTED
 *  or M0_DTML_STATE_PERSISTENT. On originator the state of the distributed
 *  transaction(dtx) can be M0_DTML_STATE_INPROGRESS to M0_DTML_STATE_PERSISTENT
 *  for each participant.
 *
 *  During recovery operation of any of the participant, rest of the participant
 *  and originator will iterate over the logged journal and extract the state
 *  of each transation for participant under recovery from logged information
 *  and will send redo request for those distributed transaction(dtx) which  are
 *  not M0_DTML_STATE_PERSISTENT on participant being recovered.
 *
 *  Upon receiving redo request participant under recovery will log the state
 *  of same distributed transaction(dtx) of remote participant in persistent
 *  store.
 *
 * @section Usecases
 *
 * 1. There is a dedicated function m0_be_dtm0_log_create which will create
 *    m0_be_dtm0_log during mkfs procedure.
 *
 * 2. When distributed transaction(dtx) successfully executed on participant,
 *    DTM0 log record will be added by participant to persistent store and where
 *    participant state in group of state will be M0_DTML_STATE_EXECUTED and for
 *    rest of the participant state will be logged as it is.
 * {
 *    struct m0_be_tx_credit     cred;
 *    struct m0_be_dtm0_log     *log;
 *    struct m0_be_tx           *tx;
 *    struct m0_dtm0_txr        *txr;
 *    struct m0_be_seg          *seg;
 *
 *    m0_be_dtm0_log_credit(M0_DTML_EXECUTED, tx, seg, &cred);
 *    m0_be_tx_open(tx, cred);
 *
 *    ...
 *    m0_be_dtm0_log_update(log, tx, txr);
 *    tx_close(tx);
 * }
 *
 * 3. When distributed transaction(dtx) become persistent on particular
 *    participant, the state of the distributed transaction(dtx) for this
 *    participant will be updated as M0_DTML_STATE_PERSISTENT.
 *
 * {
 *    struct m0_be_tx_credit     cred;
 *    struct m0_be_dtm0_log     *log;
 *    struct m0_be_tx           *tx;
 *    struct m0_dtm0_txr        *txr;
 *    struct m0_be_seg          *seg;
 *
 *    m0_be_dtm0_log_credit(M0_DTML_PERSISTENT, tx, seg, &cred);
 *    m0_be_tx_open(tx, cred);
 *
 *    ...
 *    m0_be_dtm0_log_update(log, tx, txr);
 *    tx_close(tx);
 * }
 *
 * 4. When distributed transaction(dtx) become persistent on remote participant,
 *    the participant sends the persistent notice to rest of the participants
 *    and originator stating that distributed transaction(dtx) is persistent on
 *    its store. Upon receiving persistent notice each of the participants and
 *    originator will update the state of distributed transaction(dtx) for
 *    remote participant as M0_DTML_STATE_PERSISTENT
 *    in DTM0 log record.
 *
 *    There is one special case where a process got a persistent notice from
 *    another process but the corresponding request and reply were not
 *    received/processed yet. In this case, the DTM service should add a special
 *    log record: a log record with empty payload (NULL). The users of DTM0 log
 *    should be ready to encounter such a log entry, and treat it differently
 *    whenever it is required.
 *
 * {
 *    struct m0_be_tx_credit     cred;
 *    struct m0_be_dtm0_log     *log;
 *    struct m0_be_tx           *tx;
 *    struct m0_dtm0_txr        *txr;
 *    struct m0_be_seg          *seg;
 *
 *    m0_be_dtm0_log_credit(M0_DTML_PERSISTENT, tx, seg, &cred);
 *    m0_be_tx_prep(tx, &cred);
 *    m0_be_tx_open(tx);
 *
 *    ...
 *    m0_be_dtm0_log_update(log, tx, txr);
 *    tx_close(tx);
 * }
 *
 *
 */

/* Participant state */
enum m0_dtm0_log_pa_state {
	M0_DTML_STATE_INPROGRESS,
	M0_DTML_STATE_EXECUTED,
	M0_DTML_STATE_PERSISTENT,
};

enum m0_be_dtm0_log_credit_op {
	M0_DTML_CREATE,
	M0_DTML_SENT,
	M0_DTML_EXECUTED,
	M0_DTML_PERSISTENT,
	M0_DTML_REDO
};

/* Unique identifier for request */
struct m0_dtm0_dtx_id {
	struct m0_fid di_fid;
	uint64_t      di_ts;
};

struct m0_dtm0_log_pa {
	struct m0_fid             pfid;
	enum m0_dtm0_log_pa_state pstate;
};

/* TODO: define in a separate header. */
struct m0_dtm0_txr {
	struct m0_dtm0_dtx_id  dt_tid;
	struct m0_dtm0_log_pa *dt_participants;
	uint16_t               dt_participants_nr;
	struct m0_buf          dt_txr_payload;
};

struct m0_dtm0_log_record {
	struct m0_dtm0_txr     dlr_txr;
	struct m0_be_list_link dlr_link; /* link into m0_be_dtm0_log::list */
	struct m0_list_link    dlr_tlink;
};

struct m0_be_dtm0_log {
	struct m0_mutex    dl_lock;  /* volatile structure */
	struct m0_be_list *dl_list;  /* persistent structure */
	struct m0_list    *dl_vlist; /* Volatile list */
};

// init/fini (for volatile fields)
M0_INTERNAL void m0_be_dtm0_log_init(struct m0_be_dtm0_log *log);
M0_INTERNAL void m0_be_dtm0_log_fini(struct m0_be_dtm0_log *log);

// credit interface
M0_INTERNAL void m0_be_dtm0_log_credit(enum m0_be_dtm0_log_credit_op op,
                                       struct m0_be_tx              *tx,
                                       struct m0_be_seg             *seg,
                                       struct m0_be_tx_credit       *accum);
// create/destroy
M0_INTERNAL void m0_be_dtm0_log_create(struct m0_be_tx i      *tx,
                                       struct m0_be_seg       *seg,
                                       struct m0_be_dtm0_log **out);

M0_INTERNAL void m0_be_dtm0_log_destroy(struct m0_be_dtm0_log **log,
                                        struct m0_be_tx        *tx);

// operational interfaces
M0_INTERNAL void m0_be_dtm0_log_update(struct m0_be_dtm0_log *log,
                                       struct m0_be_tx       *tx,
                                       struct m0_dtm0_txr    *txr);

M0_INTERNAL int m0_be_dtm0_log_find(struct m0_be_dtm0_log       *log,
                                    const struct m0_dtm0_dtx_id *id,
                                    struct m0_dtm0_log_record   *out);
/*
 *       0  -- if left == right
 *      -1  -- if left <  right
 *       1  -- if left >  right
 */
M0_INTERNAL int m0_dtm0_dtx_id_cmp(struct m0_dtm0_dtx_id *left,
                                   struct m0_dtm0_dtx_id *right);
#endif /* __MOTR_BE_DTM0_LOG_H__ */
