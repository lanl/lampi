/*
 * Copyright 2002-2003.  The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

extern "C" {
#include <vapi_common.h>
}

#undef PAGESIZE
#include "path/ib/sendFrag.h"
#include "queue/globals.h"

inline bool ibSendFragDesc::get_remote_ud_info(VAPI_ud_av_hndl_t *ah, VAPI_qp_num_t *qp)
{
    VAPI_ud_av_hndl_t ud_ah;
    ib_ud_peer_info_t *udpi = 
        &(ib_state.ud_peers.info[ib_state.ud_peers.proc_entries * globalDestID_m]);
    int *next_remote_hca = &(ib_state.ud_peers.next_remote_hca[globalDestID_m]);
    int *next_remote_port = &(ib_state.ud_peers.next_remote_port[globalDestID_m]);
    int i, j, k = 0, remote_hca, remote_port;
    bool found_dest_info = false;
    
    for (i = 0; i < ib_state.ud_peers.max_hcas; i++) {
        remote_hca = (i + *next_remote_hca) % ib_state.ud_peers.max_hcas;
        for (j = 0; j < ib_state.ud_peers.max_ports; j++) {
            remote_port = (j + *next_remote_port) % ib_state.ud_peers.max_ports;
            k = ((remote_hca * ib_state.ud_peers.max_ports) + remote_port);
            if (PEER_INFO_IS_VALID(udpi[k])) {
                found_dest_info = true;
                *next_remote_port = remote_port + 1;
                if (*next_remote_port >= ib_state.ud_peers.max_ports) {
                    *next_remote_port = 0;
                    *next_remote_hca = remote_hca + 1;
                    if (*next_remote_hca >= ib_state.ud_peers.max_hcas) {
                        *next_remote_hca = 0;
                    }
                }
                break;
            }
        }
    }
    
    if (!found_dest_info) {
        ulm_warn(("ibSendFragDesc::get_remote_ud_info trying to send to process"
            " %d no UD peer info found!\n", globalDestID_m));
        return false;
    }

    ib_hca_state_t *h = &(ib_state.hca[hca_index_m]);
    ud_ah = h->ud.ah_cache.get(globalDestID_m, port_index_m, udpi[k].lid);

    if (ud_ah == VAPI_INVAL_HNDL) {
        VAPI_ret_t vapi_result;
        VAPI_ud_av_t ud_av;

        // create address handle, if possible
        ud_av.dlid = udpi[k].lid;
        // set static rate setting to 100% for matched links
        ud_av.static_rate = 0;
        // don't use global routing header
        ud_av.grh_flag = 0;
        // convert from port array index to port number
        ud_av.port = port_index_m + 1;
        // set service level to 0
        ud_av.sl = 0;
        // set source path bits to 0
        ud_av.src_path_bits = 0;

        vapi_result = VAPI_create_addr_hndl(h->handle, h->pd, &ud_av, &ud_ah);
        if (vapi_result != VAPI_OK) {
            ulm_warn(("ibSendFragDesc::get_remote_ud_info VAPI_create_addr_hndl for HCA %d "
                "returned %s\n", hca_index_m, VAPI_strerror(vapi_result)));
            return false;
        }
        if (!h->ud.ah_cache.store(ud_ah, globalDestID_m, port_index_m, udpi[k].lid)) {
            ulm_warn(("ibSendFragDesc::get_remote_ud_info can't store ud_av_hndl!\n"));
            return false;
        }
    }

    *ah = ud_ah;
    *qp = udpi[k].qp_num;
    return true;
}

inline unsigned int ibSendFragDesc::pack_buffer(void *dest)
{
    /* contiguous data */
    if (tmapIndex_m < 0) {
        if (ib_state.checksum) {
            if (usecrc()) {
                return bcopy_uicrc(((unsigned char *)parentSendDesc_m->addr_m + seqOffset_m), 
                    dest, length_m, length_m);
            }
            else {
                return bcopy_uicsum(((unsigned char *)parentSendDesc_m->addr_m + seqOffset_m), 
                    dest, length_m, length_m);
            }
        }
        else {
            MEMCOPY_FUNC(((unsigned char *)parentSendDesc_m->addr_m + seqOffset_m), 
                dest, length_m);
            return 0;
        }
    }

    /* non-contiguous data */
    unsigned char *src_addr, *dest_addr = (unsigned char *)dest;
    size_t len_to_copy, len_copied;
    ULMType_t *dtype = parentSendDesc_m->datatype;
    ULMTypeMapElt_t *tmap = dtype->type_map;
    int dtype_cnt, ti;
    int tm_init = tmapIndex_m;
    int init_cnt = seqOffset_m / dtype->packed_size;
    int tot_cnt = parentSendDesc_m->posted_m.length_m / dtype->packed_size;
    unsigned char *start_addr = (unsigned char *)parentSendDesc_m->addr_m + init_cnt * dtype->extent;
    unsigned int csum = 0, ui1 = 0, ui2 = 0;


    // handle first typemap pair
    src_addr = start_addr
        + tmap[tm_init].offset
        - init_cnt * dtype->packed_size - tmap[tm_init].seq_offset + seqOffset_m;
    len_to_copy = tmap[tm_init].size
        + init_cnt * dtype->packed_size + tmap[tm_init].seq_offset - seqOffset_m;
    len_to_copy = (len_to_copy > length_m) ? length_m : len_to_copy;

    if (ib_state.checksum) {
        if (usecrc()) {
            csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, CRC_INITIAL_REGISTER);
        }
        else {
            csum = bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
        }
    }
    else {
        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
    }
    len_copied = len_to_copy;

    tm_init++;
    for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
        for (ti = tm_init; ti < dtype->num_pairs; ti++) {
            src_addr = start_addr + tmap[ti].offset;
            dest_addr = (unsigned char *)dest + len_copied;
            len_to_copy = (length_m - len_copied >= tmap[ti].size) ?
                tmap[ti].size : length_m - len_copied;
            if (len_to_copy == 0) {
                return csum;
            }

            if (ib_state.checksum) {
                if (usecrc()) {
                    csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, csum);
                }
                else {
                    csum += bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
                }
            }
            else {
                MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
            }
            len_copied += len_to_copy;
        }

        tm_init = 0;
        start_addr += dtype->extent;
    }

    return csum;
}

inline bool ibSendFragDesc::ud_init(void)
{
    bool need_frag_seq, need_tmap_index;
    VAPI_ud_av_hndl_t remote_ah;
    VAPI_qp_num_t remote_qp;

    if ((state_m & INITCOMPLETE) != 0) {
        return true;
    }

    // get remote address handle, if we can or bail...
    if (!get_remote_ud_info(&remote_ah, &remote_qp)) {
        return false;
    }

    sr_desc_m.remote_ah = remote_ah;
    sr_desc_m.remote_qp = remote_qp;
    sr_desc_m.remote_qkey = ib_state.qkey;

    // from here on out, we will complete the initialization successfully...

    // figure out the length and sequential offset of this fragment
    // also figure out if we need to get a fragment sequence number
    switch (type_m) {
        case MESSAGE_DATA:
            seqOffset_m = (parentSendDesc_m->pathInfo.ib.allocated_offset_m < 0) ? 0 :
                parentSendDesc_m->pathInfo.ib.allocated_offset_m;
            length_m = ((parentSendDesc_m->posted_m.length_m - seqOffset_m) > IB2KMSGDATABYTES) ?
                IB2KMSGDATABYTES : (parentSendDesc_m->posted_m.length_m - seqOffset_m);
            // update allocated sequential offset for message
            parentSendDesc_m->pathInfo.ib.allocated_offset_m = seqOffset_m + length_m;
            need_frag_seq = (ib_state.ack || ((parentSendDesc_m->sendType == 
                ULM_SEND_SYNCHRONOUS) && (seqOffset_m == 0))) ? true : false;
            need_tmap_index = ((parentSendDesc_m->datatype == NULL) || 
                (parentSendDesc_m->datatype->layout == CONTIGUOUS)) ? false : true;
            break;
        case MESSAGE_DATA_ACK:
            length_m = sizeof(ibDataAck_t);
            seqOffset_m = 0;
            need_frag_seq = false;
            need_tmap_index = false;
            break;
        default:
            ulm_err(("ibSendFragDesc::init() unrecognized send fragment type "
            "%d\n", type_m));
            return false;
    }
    
#ifdef ENABLE_RELIABILITY
    // assign fragment sequence number(s) if we need them...
    frag_seq_m = 0;

    if (need_frag_seq) { 
        if (usethreads())
            reliabilityInfo->next_frag_seqsLock[globalDestID_m].lock();
        frag_seq_m = (reliabilityInfo->next_frag_seqs[globalDestID_m])++;
        if (usethreads())
            reliabilityInfo->next_frag_seqsLock[globalDestID_m].unlock();
    }
#else
    frag_seq_m = 0;
#endif

    tmapIndex_m = -1;

    if (need_tmap_index) {
        // calculate type map index based on this fragment's offset...
        int dtype_cnt = seqOffset_m / parentSendDesc_m->datatype->packed_size;
        size_t data_copied = dtype_cnt * parentSendDesc_m->datatype->packed_size;
        ssize_t data_remaining = (ssize_t)(seqOffset_m - data_copied);
        tmapIndex_m = parentSendDesc_m->datatype->num_pairs - 1;
        for (int ti = 0; ti < parentSendDesc_m->datatype->num_pairs; ti++) {
            if (parentSendDesc_m->datatype->type_map[ti].seq_offset == data_remaining) {
                tmapIndex_m = ti;
                break;
            } else if (parentSendDesc_m->datatype->type_map[ti].seq_offset > data_remaining) {
                tmapIndex_m = ti - 1;
                break;
            }
        }
    }

    // copy data or control msg. payload to registered memory
    switch (type_m) {
        case MESSAGE_DATA:
            {
                ibData2KMsg *p = (ibData2KMsg *)((unsigned long)sg_m[0].addr);
                p->header.common.type = MESSAGE_DATA;
                p->header.ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE(parentSendDesc_m->ctx_m,
                    (parentSendDesc_m->sendType == ULM_SEND_SYNCHRONOUS) ? 
                    MSGTYPE_PT2PT_SYNC : MSGTYPE_PT2PT);
                p->header.tag_m = parentSendDesc_m->posted_m.tag_m;
                p->header.senderID = myproc();
                p->header.destID = globalDestID_m;
                p->header.dataLength = length_m;
                p->header.msgLength = parentSendDesc_m->posted_m.length_m;
                p->header.frag_seq = frag_seq_m;
                p->header.isendSeq_m = parentSendDesc_m->isendSeq_m;
                p->header.sendFragDescPtr.ptr = this;
                p->header.dataSeqOffset = seqOffset_m;
                p->header.dataChecksum = (length_m != 0) ? pack_buffer(p->data) : 0;
                if (ib_state.checksum) {
                    p->header.checksum = (usecrc()) ? uicrc(p, sizeof(ibDataHdr_t) - 
                        sizeof(ulm_uint32_t)) : uicsum(p, sizeof(ibDataHdr_t) - 
                        sizeof(ulm_uint32_t));
                }
                else {
                    p->header.checksum = 0;
                }
                // now set the real send length
                sg_m[0].len = sizeof(ibDataHdr_t) + length_m;
            }
            break;
        case MESSAGE_DATA_ACK:
            {
                // everything should be filled out already by AckData()
                // so we only need to calculate the checksum, if wanted...
                ibDataAck_t *p = (ibDataAck_t *)((unsigned long)sg_m[0].addr);
                p->checksum = 0;
                if (ib_state.checksum) {
                    if (usecrc()) {
                        p->checksum = uicrc(p, length_m - sizeof(ulm_uint32_t));
                    }
                    else {
                        p->checksum = uicsum(p, length_m - sizeof(ulm_uint32_t));
                    }
                }
                // now set the real send length
                sg_m[0].len = sizeof(ibDataAck_t);
            }
            break;
        default:
            break;
    }

    // mark this fragment descriptor as ready to post
    state_m = (state)(state_m | INITCOMPLETE);

    return true;
}

bool ibSendFragDesc::init(void)
{
    switch (qp_type_m) {
        case UD_QP:
            return ud_init();
        // we only support UD traffic for now...
        case RC_QP:
        case NOTASSIGNED_QP:
        default:
            ulm_exit((-1, "ibSendFragDesc::init() qp_type_m "
            "is %d (not supported UD_QP %d)\n", qp_type_m, UD_QP));
            // to quiet compilers..
            return false;
    }
}

bool ibSendFragDesc::init(SendDesc_t *message, int hca, int port)
{
    if ((state_m & BASICINFO) == 0) {
        type_m = MESSAGE_DATA;
        hca_index_m = hca;
        port_index_m = port;
        timeSent_m = -1.0;
        numTransmits_m = 0;
        parentSendDesc_m = message;
        globalDestID_m = communicators[message->ctx_m]->remoteGroup->
            mapGroupProcIDToGlobalProcID[message->posted_m.peer_m];
        state_m = (state)(state_m | BASICINFO);
    }
    switch (qp_type_m) {
        case UD_QP:
            return ud_init();
        // we only support UD traffic for now...
        case RC_QP:
        case NOTASSIGNED_QP:
        default:
            ulm_exit((-1, "ibSendFragDesc::init() qp_type_m "
            "is %d (not supported UD_QP %d)\n", qp_type_m, UD_QP));
            // to quiet compilers..
            return false;
    }
}

bool ibSendFragDesc::init(enum ibCtlMsgTypes type, int glDestID, 
int hca, int port)
{
    if (qp_type_m != UD_QP) {
        ulm_exit((-1, "ibSendFragDesc::init() control msg. qp_type_m "
        "is %d (not UD_QP: %d hca: %d send_frag_avail: %d)\n", qp_type_m, UD_QP,
        hca, ib_state.hca[hca].send_frag_avail));
        return false;
    }

    if ((state_m & BASICINFO) == 0) {
        type_m = type;
        hca_index_m = hca;
        port_index_m = port;
        timeSent_m = -1.0;
        numTransmits_m = 0;
        parentSendDesc_m = 0;
        globalDestID_m = glDestID;
        frag_seq_m = 0;
        state_m = (state)(state_m | BASICINFO);
    }

    // all control messages use the UD transport
    return ud_init();
}

bool ibSendFragDesc::post(double timeNow, int *errorCode)
{
    VAPI_ret_t vapi_result;
    ib_hca_state_t *h = &(ib_state.hca[hca_index_m]);
    bool locked_here = false;
    bool got_tokens = false;

    *errorCode = ULM_SUCCESS;

    if (usethreads() && !ib_state.locked) {
        ib_state.lock.lock();
        ib_state.locked = true;
        locked_here = true;
    }

    if ((state_m & POSTED) == 0) {
        // try to get tokens and send...
        if ((h->ud.sq_tokens >= 1) && (h->send_cq_tokens >= 1)) {
            (h->ud.sq_tokens)--;
            (h->send_cq_tokens)--;
            got_tokens = true;
        } 
        if (got_tokens) {
            vapi_result = VAPI_post_sr(h->handle, h->ud.handle, &sr_desc_m);
            if (vapi_result == VAPI_OK) {
                state_m = (state)(state_m | POSTED);
                if (locked_here) {
                    ib_state.locked = false;
                    ib_state.lock.unlock();
                }
                return true;
            }
            else {
                (h->ud.sq_tokens)++;
                (h->send_cq_tokens)++;
                ulm_warn(("ibSendFragDesc::post send of desc. %p failed with %s\n",
                    this, VAPI_strerror(vapi_result)));
            }
        }
    }
#ifdef ENABLE_RELIABILITY
    else if ((state_m & LOCALACKED) != 0) {
        // try to get tokens and resend...
        if ((h->ud.sq_tokens >= 1) && (h->send_cq_tokens >= 1)) {
            (h->ud.sq_tokens)--;
            (h->send_cq_tokens)--;
            got_tokens = true;
        } 
        if (got_tokens) {
            vapi_result = VAPI_post_sr(h->handle, h->ud.handle, &sr_desc_m);
            if (vapi_result == VAPI_OK) {
                state_m = (state)(state_m & ~LOCALACKED);
                if (locked_here) {
                    ib_state.locked = false;
                    ib_state.lock.unlock();
                }
                return true;
            }
            else {
                (h->ud.sq_tokens)++;
                (h->send_cq_tokens)++;
                ulm_warn(("ibSendFragDesc::post resend of desc. %p failed with %s\n", 
                    this, VAPI_strerror(vapi_result)));
            }
        }
    }
#endif
    if (locked_here) {
        ib_state.locked = false;
        ib_state.lock.unlock();
    }
    return false;
}
