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
#include <vapi.h>
#include <vapi_common.h>
}

#include <string.h>
#include <netinet/in.h>

#include "ulm/ulm.h"
#include "path/ib/init.h"
#include "internal/malloc.h"
#include "internal/state.h"

#undef PAGESIZE
#include "internal/system.h"
#include "init/init.h"
#include "client/adminMessage.h"
#include "path/ib/recvFrag.h"
#include "path/ib/sendFrag.h"
#include "path/ib/state.h"
#include "util/DblLinkList.h"

extern FixedSharedMemPool SharedMemoryPools;

void ibSetup(lampiState_t *s)
{
    VAPI_ret_t vapi_result;
    VAPI_qp_init_attr_t qpinit;
    VAPI_qp_attr_t qpattr;
    VAPI_qp_attr_mask_t qpattrmask;
    VAPI_mr_t mr;
    IB_port_t p;
    ibRecvFragDesc *rfrag;
    ibSendFragDesc *sfrag;
    DoubleLinkList dlist;
    bool mcast_attached = false;
    int i, j, rc, usable = 0, mcast_prefix = htonl(0xff120000);
    int *authdata, *exchange_recv, *exchange_send;
    int maxhcas, maxports, num_rbufs, num_sbufs;
    void *rmem, *smem;

    int nFreeLists;
    long nPagesPerList;
    ssize_t PoolChunkSize;
    size_t PageSize;
    ssize_t ElementSize;
    long minPagesPerList;
    long maxPagesPerList;
    long mxConsecReqFailures;
    int retryForMoreResources;
    bool enforceMemAffinity;
    bool Abort;
    int threshToGrowList;

    exchange_send = (int *)ulm_malloc(sizeof(int) * 2);
    exchange_send[0] = exchange_send[1] = 0;
    ib_state.num_hcas = 0;
    ib_state.num_active_hcas = 0;

    // set some defaults here...
    ib_state.max_ud_2k_buffers = 2048;
    ib_state.max_outst_frags = -1;

    // client daemon does info exchanges only
    if (s->iAmDaemon) {
        goto exchange_info;
    }

    // get the number of available InfiniBand HCAs
    vapi_result = EVAPI_list_hcas(0, &ib_state.num_hcas, (VAPI_hca_id_t *)NULL);
    
    // no available HCAs...active_port is zero, exchange info now
    if (ib_state.num_hcas == 0) {
        goto exchange_info;
    }

    // only use first LAMPI_MAX_IB_HCAS HCAs
    if (ib_state.num_hcas > LAMPI_MAX_IB_HCAS) {
        ulm_warn(("%d InfiniBand HCAs available, LAMPI_MAX_IB_HCAS set to %d\n",
            ib_state.num_hcas, LAMPI_MAX_IB_HCAS));
        ib_state.num_hcas = LAMPI_MAX_IB_HCAS;
    }

    // get ids
    vapi_result = EVAPI_list_hcas(
        sizeof(VAPI_hca_id_t) * ib_state.num_hcas,
        &(ib_state.num_hcas),
        ib_state.hca_ids);
    if (vapi_result != VAPI_OK) {
        ulm_err(("ibSetup: EVAPI_list_hcas() returned %s\n", 
            VAPI_strerror(vapi_result)));
        exit(1);
    }

    // get handles and initialize port info array to zero...
    for (i = 0; i < (int)ib_state.num_hcas; i++) {
        vapi_result = EVAPI_get_hca_hndl(ib_state.hca_ids[i], 
            &(ib_state.hca[i].handle));
        if (vapi_result == VAPI_OK) {
            ib_state.hca[i].usable = true;
            usable++;
        }
        else {
            ib_state.hca[i].usable = false;
        }
        ib_state.hca[i].num_active_ports = 0;
    }

    if (usable == 0) {
        goto exchange_info;
    }

    // get HCA capabilities and vendor info...allocate port 
    // cap arrays and get port info
    for (i = 0; i < (int)ib_state.num_hcas; i++) {
        bool hca_active = false;
        if (ib_state.hca[i].usable) {
            vapi_result = VAPI_query_hca_cap(
                ib_state.hca[i].handle, 
                &(ib_state.hca[i].vendor),
                &(ib_state.hca[i].cap));
            if (vapi_result != VAPI_OK) {
                ulm_err(("ibSetup: VAPI_query_hca_cap() for HCA %d returned %s\n", 
                    i, VAPI_strerror(vapi_result)));
                    exit(1);
            }

            // only use first LAMPI_MAX_IB_HCA_PORTS of an HCA
            IB_port_t num_ports = (ib_state.hca[i].cap.phys_port_num > 
                (IB_port_t)LAMPI_MAX_IB_HCA_PORTS) ?
                (IB_port_t)LAMPI_MAX_IB_HCA_PORTS : ib_state.hca[i].cap.phys_port_num;

            // get the port capabilities
            for (p = 0; p < num_ports; p++) {
                // VAPI starts port indices at 1
                vapi_result = VAPI_query_hca_port_prop(ib_state.hca[i].handle, (p + 1),
                    &(ib_state.hca[i].ports[p]));
                if (vapi_result != VAPI_OK) {
                    ulm_err(("ibSetup: VAPI_query_hca_port_cap() for HCA %d port %d returned %s\n", 
                        i, (p + 1), VAPI_strerror(vapi_result)));
                    exit(1);
                }
                // count the port if it's active and has its MTU set to at least 2KB
                if ((ib_state.hca[i].ports[p].state == PORT_ACTIVE) && 
                    (ib_state.hca[i].ports[p].max_mtu >= MTU2048)) {
                    ib_state.hca[i].active_ports[ib_state.hca[i].num_active_ports++] = p;
                    hca_active = true;
                }
            }
            // set the HCA as active if there is at least one active port
            if (hca_active) {
                ib_state.active_hcas[ib_state.num_active_hcas++] = i;
                // record max number of active ports per HCA for first allgather()...see exchange_info
                if (ib_state.hca[i].num_active_ports > exchange_send[1]) {
                    exchange_send[1] = ib_state.hca[i].num_active_ports;
                }
            }
        }
    } 
    
    if (ib_state.num_active_hcas == 0) {
        goto exchange_info;
    }

    authdata = s->client->getAuthData();
    // set Q_Key to timestamp from mpirun...
    ib_state.qkey = (VAPI_qkey_t)authdata[2];
    // set mcast_gid to value constructed from mpirun authorization data...
    // all process must be running on the same endian type of machine 
    // for this to work...
    memcpy(&(ib_state.mcast_gid), &mcast_prefix, 4);
    memcpy(&(ib_state.mcast_gid[4]), authdata, 12);

    for (i = 0; i < ib_state.num_active_hcas; i++) {
        ib_hca_state_t *h = &(ib_state.hca[ib_state.active_hcas[i]]);
        // allocate protection domain for HCA
        vapi_result = VAPI_alloc_pd(h->handle, &(h->pd));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_alloc_pd() for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // allocate receive completion queue for HCA
        vapi_result = VAPI_create_cq(h->handle, h->cap.max_num_ent_cq,
            &(h->recv_cq), &(h->recv_cq_size));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_cq() [recv] for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // set number of CQ tokens to size of CQ
        h->recv_cq_tokens = h->recv_cq_size;
        // allocate send completion queue for HCA
        vapi_result = VAPI_create_cq(h->handle, h->cap.max_num_ent_cq,
            &(h->send_cq), &(h->send_cq_size));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_cq() [send] for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // set number of CQ tokens to size of CQ
        h->send_cq_tokens = h->send_cq_size;
        // initialize UD QP information to use PD and CQs just created
        qpinit.sq_cq_hndl = h->send_cq;
        qpinit.rq_cq_hndl = h->recv_cq;
        qpinit.cap.max_oust_wr_sq = h->cap.max_qp_ous_wr;
        qpinit.cap.max_oust_wr_rq = h->cap.max_qp_ous_wr;
        qpinit.cap.max_sg_size_sq = h->cap.max_num_sg_ent;
        qpinit.cap.max_sg_size_rq = h->cap.max_num_sg_ent;
        qpinit.rdd_hndl = VAPI_INVAL_HNDL;
        qpinit.sq_sig_type = VAPI_SIGNAL_ALL_WR;
        qpinit.rq_sig_type = VAPI_SIGNAL_ALL_WR;
        qpinit.pd_hndl = h->pd;
        qpinit.ts_type = VAPI_TS_UD;
        // allocate a single UD QP
        vapi_result = VAPI_create_qp(h->handle, &qpinit, 
            &(h->ud.handle), &(h->ud.prop));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_qp() for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // set Q_Key for QP and transition to INIT state
        qpattr.qp_state = VAPI_INIT;
        qpattr.qkey = ib_state.qkey;
        // the partition key index and port are for RC/UC only -- but
        // empirical evidence shows otherwise
        qpattr.pkey_ix = 0;
        qpattr.port = h->active_ports[0] + 1;
        QP_ATTR_MASK_CLR_ALL(qpattrmask);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QKEY);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QP_STATE);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_PKEY_IX);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_PORT);
        vapi_result = VAPI_modify_qp(h->handle, h->ud.handle, 
            &(qpattr), &(qpattrmask), &(h->ud.prop.cap));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_modify_qp() RST->INIT for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // start multicast support on only UD QP on one HCA...
        // last arg mcast dlid currently ignored
        if (!mcast_attached) {
            vapi_result = VAPI_attach_to_multicast(h->handle, 
                ib_state.mcast_gid, h->ud.handle, (IB_lid_t)0);
            if (vapi_result != VAPI_OK) {
                ulm_err(("ibSetup: VAPI_attach_to_multicast for HCA %d returned %s\n",
                    ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
                exit(1);
            }
            h->ud.receive_multicast = true;
            mcast_attached = true;
        }
        else {
            h->ud.receive_multicast = false;
        }
        // transition the QP to RTR 
        qpattr.qp_state = VAPI_RTR;
        QP_ATTR_MASK_CLR_ALL(qpattrmask);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QP_STATE);
        vapi_result = VAPI_modify_qp(h->handle, h->ud.handle, 
            &(qpattr), &(qpattrmask), &(h->ud.prop.cap));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_modify_qp() INIT->RTR for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // transition the QP to RTS (ready to use)
        qpattr.qp_state = VAPI_RTS;
        qpattr.sq_psn = 1;
        QP_ATTR_MASK_CLR_ALL(qpattrmask);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QP_STATE);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_SQ_PSN);
        vapi_result = VAPI_modify_qp(h->handle, h->ud.handle, 
            &(qpattr), &(qpattrmask), &(h->ud.prop.cap));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_modify_qp() RTR->RTS for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // set the SQ and RQ tokens to the number of max outstanding WR allowed
        h->ud.sq_tokens = h->ud.prop.cap.max_oust_wr_sq;
        h->ud.rq_tokens = h->ud.prop.cap.max_oust_wr_rq;

        // calculate an appropriate number of 2KB receive buffers to track
        num_rbufs = ib_state.max_ud_2k_buffers;
        if ((u_int32_t)num_rbufs > h->ud.rq_tokens) 
            num_rbufs = h->ud.rq_tokens;
        if ((u_int32_t)num_rbufs > h->recv_cq_tokens)
            num_rbufs = h->recv_cq_tokens;

        // calculate an appropriate number of 2KB send buffers to track
        num_sbufs = ib_state.max_ud_2k_buffers;
        if ((u_int32_t)num_sbufs > h->ud.sq_tokens) 
            num_sbufs = h->ud.sq_tokens;
        if ((u_int32_t)num_sbufs > h->send_cq_tokens)
            num_sbufs = h->send_cq_tokens;

        // reset the SQ and RQ tokens to new, possibly lower, limit
        h->ud.sq_tokens = num_sbufs;
        h->ud.rq_tokens = num_rbufs;
        // cache value of send and receive num_bufs for later reference
        h->ud.num_rbufs = num_rbufs;
        h->ud.num_sbufs = num_sbufs;

        // grab a hunk of memory...make sure it is page-aligned...
        // register it as a memory region for receive buffers
        rmem = ulm_malloc(num_rbufs * (2048 + IB_GRH_LEN) + getpagesize());
        rmem = (void *)(((unsigned long)rmem + getpagesize() - 1)/getpagesize());
        rmem = (void *)((unsigned long)rmem * getpagesize());
        // cache start of buffer memory for later calculations
        h->ud.base_rbuf = rmem;

        // do the same for send buffers
        smem = ulm_malloc(num_sbufs * 2048 + getpagesize());
        smem = (void *)(((unsigned long)smem + getpagesize() - 1)/getpagesize());
        smem = (void *)((unsigned long)smem * getpagesize());
        // cache start of buffer memory for later calculations
        h->ud.base_sbuf = smem;

        // initialize a receive fragment descriptor list per HCA -- will eventually
        // have its memoryPool chunks registered with the HCA for RDMA write access...
        rc = h->recv_frag_list.Init(nFreeLists = 1,
            nPagesPerList = ((num_rbufs * sizeof(ibRecvFragDesc)) / getpagesize()) + 1,
            PoolChunkSize = getpagesize(),
            PageSize = getpagesize(),
            ElementSize = sizeof(ibRecvFragDesc),
            minPagesPerList = ((num_rbufs * sizeof(ibRecvFragDesc)) / getpagesize()) + 1,
            maxPagesPerList = -1,
            mxConsecReqFailures = 1000,
            "IB receive fragment descriptors",
            retryForMoreResources = true,
            NULL,
            enforceMemAffinity = false,
            NULL,
            Abort = true,
            threshToGrowList = 0); 
        if (rc) {
            ulm_exit((-1, "Error: can't initialize IB recv_frag_list for HCA %d\n",
                ib_state.active_hcas[i]));
        }

        // initialize a send fragment descriptor list per HCA 
        rc = h->send_frag_list.Init(nFreeLists = 1,
            nPagesPerList = ((num_sbufs * sizeof(ibSendFragDesc)) / getpagesize()) + 1,
            PoolChunkSize = getpagesize(),
            PageSize = getpagesize(),
            ElementSize = sizeof(ibSendFragDesc),
            minPagesPerList = ((num_sbufs * sizeof(ibSendFragDesc)) / getpagesize()) + 1,
            maxPagesPerList = -1,
            mxConsecReqFailures = 1000,
            "IB send fragment descriptors",
            retryForMoreResources = true,
            NULL,
            enforceMemAffinity = false,
            NULL,
            Abort = true,
            threshToGrowList = 0); 
        if (rc) {
            ulm_exit((-1, "Error: can't initialize IB send_frag_list for HCA %d\n",
                ib_state.active_hcas[i]));
        }

        // register the receive buffer memory
        mr.type = VAPI_MR;
        mr.start = (VAPI_virt_addr_t)((unsigned long)rmem);
        mr.size = num_rbufs * (2048 + IB_GRH_LEN);
        mr.pd_hndl = h->pd;
        mr.acl = VAPI_EN_LOCAL_WRITE;
        vapi_result = VAPI_register_mr(h->handle, &mr, &(h->ud.recv_mr_handle), 
            &(h->ud.recv_mr));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_register_mr for HCA %d (receive buffers) returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }

        // register the send buffer memory
        mr.type = VAPI_MR;
        mr.start = (VAPI_virt_addr_t)((unsigned long)smem);
        mr.size = num_sbufs * 2048;
        mr.pd_hndl = h->pd;
        mr.acl = 0;
        vapi_result = VAPI_register_mr(h->handle, &mr, &(h->ud.send_mr_handle), 
            &(h->ud.send_mr));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_register_mr for HCA %d (send buffers) returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }

        // initialize recv desc. to reference each 2KB buffer
        for (j = 0; j < num_rbufs; j++) {

            // grab receive fragment descriptor and initialize
            rfrag = h->recv_frag_list.getElement(0, rc);
            if (rc != ULM_SUCCESS) {
                ulm_err(("ibSetup: unable to get receive fragment desc. for HCA %d\n",
                    ib_state.active_hcas[i]));
                exit(1);
            }

            // initialize LA-MPI recv. frag. desc.
            rfrag->hca_index_m = ib_state.active_hcas[i];
            rfrag->qp_type_m = ibRecvFragDesc::UD_QP;

            // store pointer to receive fragment descriptor as id
            rfrag->rr_desc_m.id = (VAPI_wr_id_t)((unsigned long)rfrag);
            rfrag->rr_desc_m.opcode = VAPI_RECEIVE;
            rfrag->rr_desc_m.comp_type = VAPI_SIGNALED;
            rfrag->rr_desc_m.sg_lst_p = rfrag->sg_m;
            rfrag->rr_desc_m.sg_lst_len = 1;

            rfrag->sg_m[0].addr = (VAPI_virt_addr_t)((unsigned long)rmem);
            rfrag->sg_m[0].len = (2048 + IB_GRH_LEN);
            rfrag->sg_m[0].lkey = h->ud.recv_mr.l_key;

            rmem = (void *)((char *)rmem + (2048 + IB_GRH_LEN));
        
            // post it...
            vapi_result = VAPI_post_rr(h->handle, h->ud.handle, &(rfrag->rr_desc_m));
            if (vapi_result != VAPI_OK) {
                ulm_err(("ibSetup: VAPI_post_rr() for HCA %d (desc. num. %d) returned %s\n",
                    ib_state.active_hcas[i], j, VAPI_strerror(vapi_result)));
                exit(1);
            }

            // decrement tokens...don't need to check values...
            (h->recv_cq_tokens)--;
            (h->ud.rq_tokens)--;
        }

        // initialize send desc. to reference each 2KB buffer
        for (j = 0; j < num_sbufs; j++) {

            // grab send fragment descriptor and initialize
            sfrag = h->send_frag_list.getElement(0, rc);
            if (rc != ULM_SUCCESS) {
                ulm_err(("ibSetup: unable to get send fragment desc. for HCA %d\n",
                    ib_state.active_hcas[i]));
                exit(1);
            }

            // initialize LA-MPI send frag. desc. 
            sfrag->hca_index_m = ib_state.active_hcas[i];
            sfrag->qp_type_m = ibSendFragDesc::UD_QP;
            sfrag->state_m = ibSendFragDesc::IBRESOURCES;

            // store pointer to send fragment descriptor as id
            sfrag->sr_desc_m.id = (VAPI_wr_id_t)((unsigned long)sfrag);
            sfrag->sr_desc_m.opcode = VAPI_SEND;
            sfrag->sr_desc_m.comp_type = VAPI_SIGNALED;
            sfrag->sr_desc_m.sg_lst_p = sfrag->sg_m;
            sfrag->sr_desc_m.sg_lst_len = 1;
            sfrag->sr_desc_m.remote_qkey = ib_state.qkey;

            sfrag->sg_m[0].addr = (VAPI_virt_addr_t)((unsigned long)smem);
            sfrag->sg_m[0].len = 2048; // possibly overwritten with real len <= 2048
            sfrag->sg_m[0].lkey = h->ud.send_mr.l_key;

            smem = (void *)((char *)smem + 2048);
        
            // store it on a temporary list...
            dlist.AppendNoLock(sfrag);
        }

        // store the send descriptors on the freelist
        while (dlist.size()) {
            sfrag = (ibSendFragDesc *)dlist.GetLastElementNoLock();
            h->send_frag_list.returnElementNoLock(sfrag, 0);;
        }
    }

exchange_info:

    exchange_send[0] = ib_state.num_active_hcas;
    exchange_recv = (int *)ulm_malloc(nprocs() * sizeof(int) * 2);

    // determine max number of active HCAS and active ports per HCA
    rc = s->client->allgather(exchange_send, exchange_recv, 2 * sizeof(int));
    if (rc != ULM_SUCCESS) {
        s->error = ERROR_LAMPI_INIT_POSTFORK_IB;
        return;
    }

    maxhcas = 0;
    maxports = 0;

    for (i = 0; i < nprocs(); i++) {
        if (exchange_recv[i*2] > maxhcas) {
            maxhcas = exchange_recv[i*2];
        }
        if (exchange_recv[i*2+1] > maxports) {
            maxports = exchange_recv[i*2+1];
        }
    }

    ulm_free(exchange_send);
    ulm_free(exchange_recv);

#ifndef ENABLE_CT
    // notify mpirun of the allgather results...
    int maxsize = sizeof(ib_ud_peer_info_t);
    if ((myhost() == 0) && ((s->useDaemon && s->iAmDaemon) || 
        (!s->useDaemon && (local_myproc() == 0)))) {
        /* send max active info to mpirun */
        s->client->reset(adminMessage::SEND);
        s->client->pack(&maxhcas, adminMessage::INTEGER, 1);
        s->client->pack(&maxports, adminMessage::INTEGER, 1);
        s->client->pack(&maxsize, adminMessage::INTEGER, 1);
        s->client->send(-1, adminMessage::IBMAXACTIVE, &rc);
    }
#endif

    // don't use IB if there are no active HCAs or ports
    if ((maxhcas == 0) || (maxports == 0)) {
        s->ib = 0;
        return;
    }

    // cache size of soon-to-be-gathered array of ib_ud_qp_info_t structs
    ib_state.ud_peers.max_hcas = maxhcas;
    ib_state.ud_peers.max_ports = maxports;
    ib_state.ud_peers.proc_entries = maxhcas * maxports;
	
    // create and initialize arrays for round-robin processing
    ib_state.ud_peers.next_remote_hca = (int *)ulm_malloc(nprocs() * sizeof(int));
    ib_state.ud_peers.next_remote_port = (int *)ulm_malloc(nprocs() * sizeof(int));
    for (i = 0; i < nprocs(); i++) {
        ib_state.ud_peers.next_remote_hca[i] = 0;
        ib_state.ud_peers.next_remote_port[i] = 0;
    }

    ib_ud_peer_info_t *exchange_max_send = 
        (ib_ud_peer_info_t *)ulm_malloc(sizeof(ib_ud_peer_info_t) * 
        maxhcas * maxports);
    ib_state.ud_peers.info = 
        (ib_ud_peer_info_t *)ulm_malloc(sizeof(ib_ud_peer_info_t) * 
        nprocs() * maxhcas * maxports);
	
    if (!s->iAmDaemon) {
        // initialize exchange_max_send with this process' IB active port info...
        // mark unused entries as invalid
        for (i = 0; i < ib_state.ud_peers.max_hcas; i++) {
            for (j = 0; j < ib_state.ud_peers.max_ports; j++) {
                int l = (i * ib_state.ud_peers.max_ports) + j;
                
                exchange_max_send[l].flag = 0;
    
                if ((i < ib_state.num_active_hcas) && 
                    (j < ib_state.hca[ib_state.active_hcas[i]].num_active_ports)) {
                    int k = ib_state.active_hcas[i];
                    int m = ib_state.hca[k].active_ports[j];
    
                    exchange_max_send[l].qp_num = ib_state.hca[k].ud.prop.qp_num;
                    exchange_max_send[l].lid = ib_state.hca[k].ports[m].lid;
                    exchange_max_send[l].lmc = ib_state.hca[k].ports[m].lmc;
                    SET_PEER_INFO_VALID(exchange_max_send[l]);
                }
                else {
                    SET_PEER_INFO_INVALID(exchange_max_send[l]);
                }
            }
        }
    }

    // gather active HCA and port information from all processes
    rc = s->client->allgather(exchange_max_send, ib_state.ud_peers.info, 
        sizeof(ib_ud_peer_info_t) * ib_state.ud_peers.proc_entries);
    if (rc != ULM_SUCCESS) {
        ulm_free(exchange_max_send);
        s->error = ERROR_LAMPI_INIT_POSTFORK_IB;
        return;
    }
    
    ulm_free(exchange_max_send);

    return;
}

