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

#include <vapi.h>
#include <vapi_common.h>
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
#include "path/ib/state.h"

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
    bool mcast_attached = false;
    int i, j, rc, usable = 0, mcast_prefix = htonl(0xff120000);
    int *authdata, *exchange_recv, *exchange_send;
    int maxhcas, maxports, num_bufs;
    void *mem;

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

    // set these defaults here...eventually these will be set from
    // mpirun values
    ib_state.max_ud_2k_buffers = 2048;
    ib_state.ack = true;
    ib_state.checksum = false;

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
        QP_ATTR_MASK_CLR_ALL(qpattrmask);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QKEY);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QP_STATE);
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
        QP_ATTR_MASK_CLR_ALL(qpattrmask);
        QP_ATTR_MASK_SET(qpattrmask, QP_ATTR_QP_STATE);
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

        // calculate an appropriate number of 2KB buffers to track
        num_bufs = ib_state.max_ud_2k_buffers;
        if ((u_int32_t)num_bufs > h->ud.rq_tokens) 
            num_bufs = h->ud.rq_tokens;
        if ((u_int32_t)num_bufs > h->recv_cq_tokens)
            num_bufs = h->recv_cq_tokens;

        // initialize bufs addrLifo tracker
        h->ud.bufs = new addrLifo(num_bufs,128,256);
        // cache value of num_bufs for later reference
        h->ud.num_bufs = num_bufs;

        // grab a hunk of memory...register it as a memory region
        mem = ulm_malloc(num_bufs * 2048);
        // grab two more hunks for receive descriptors and scatter entries
        h->ud.rr_descs = (VAPI_rr_desc_t *)ulm_malloc(num_bufs * sizeof(VAPI_rr_desc_t));
        h->ud.sg = (VAPI_sg_lst_entry_t *)ulm_malloc(num_bufs * sizeof(VAPI_sg_lst_entry_t));

        // initialize a receive fragment descriptor list per HCA -- will eventually
        // have its memoryPool chunks registered with the HCA for RDMA write access...
        rc = h->recv_frag_list.Init(nFreeLists = 1,
            nPagesPerList = ((num_bufs * sizeof(ibRecvFragDesc)) / getpagesize()) + 1,
            PoolChunkSize = getpagesize(),
            PageSize = getpagesize(),
            ElementSize = sizeof(ibRecvFragDesc),
            minPagesPerList = ((num_bufs * sizeof(ibRecvFragDesc)) / getpagesize()) + 1,
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

        // register the memory
        mr.type = VAPI_MR;
        mr.start = (VAPI_virt_addr_t)((unsigned long)mem);
        mr.size = num_bufs * 2048;
        mr.pd_hndl = h->pd;
        mr.acl = VAPI_EN_LOCAL_WRITE;
        vapi_result = VAPI_register_mr(h->handle, &mr, &(h->ud.mr_handle), 
            &(h->ud.mr));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_register_mr for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // store 2KB chunks in addrLifo...and create recv_desc and sg entry
        for (j = 0; j < num_bufs; j++) {
            h->ud.bufs->push(mem);

            // grab receive fragment descriptor and initialize
            rfrag = h->recv_frag_list.getElement(0, rc);
            if (rc != ULM_SUCCESS) {
                ulm_err(("ibSetup: unable to get receive fragment desc. for HCA %d\n",
                    ib_state.active_hcas[i]));
                exit(1);
            }

            rfrag->hca_index_m = ib_state.active_hcas[i];
            rfrag->qp_type_m = ibRecvFragDesc::UD_QP;
            rfrag->desc_index_m = j;

            // store pointer to receive fragment descriptor as id
            h->ud.rr_descs[j].id = (VAPI_wr_id_t)((unsigned long)rfrag);
            h->ud.rr_descs[j].opcode = VAPI_RECEIVE;
            h->ud.rr_descs[j].comp_type = VAPI_SIGNALED;
            h->ud.rr_descs[j].sg_lst_p = &(h->ud.sg[j]);
            h->ud.rr_descs[j].sg_lst_len = 1;

            h->ud.sg[j].addr = (VAPI_virt_addr_t)((unsigned long)mem);
            h->ud.sg[j].len = 2048;
            h->ud.sg[j].lkey = h->ud.mr.l_key;

            mem = (void *)((char *)mem + 2048);
        }

        // now post them all...
        vapi_result = EVAPI_post_rr_list(h->handle, h->ud.handle,
            h->ud.num_bufs, h->ud.rr_descs);
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: EVAPI_post_rr_list() for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
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

