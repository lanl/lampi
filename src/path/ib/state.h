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

#ifndef IB_STATE_H
#define IB_STATE_H

extern "C" {
#include <vapi.h>
}

#include <sys/types.h>
#include <unistd.h>
#include "mem/FreeLists.h"
#include "util/Lock.h"
#undef PAGESIZE
#include "path/ib/header.h"

#define LAMPI_MAX_IB_HCA_PORTS  2
#define LAMPI_MAX_IB_HCAS       2

/* forward declarations */
class ibRecvFragDesc;
class ibSendFragDesc;

/* simple cache object to keep track of address
 * handles for address vectors used in UD communication
 */
class ib_ud_av_cache {
    struct info {
        int egress_port_index;
        VAPI_ud_av_hndl_t ah; 
        IB_lid_t dlid;
        struct info *next;
    };
        
    public:
        struct info *cache;

        ib_ud_av_cache() { cache = 0; }

        VAPI_ud_av_hndl_t get(int dest, int pindex, IB_lid_t dlid)
        {
            VAPI_ud_av_hndl_t result = VAPI_INVAL_HNDL;
            struct info *p;

            if (cache == 0) {
                return result;
            }

            p = &(cache[dest]);
            do {
                if ((p->egress_port_index == pindex) && 
                    (p->dlid == dlid)) {
                    result = p->ah;
                    break;      
                } 
                p = p->next;
            } while (p);
            
            return result;
        }

        bool store(VAPI_ud_av_hndl_t ah, int dest, int pindex, IB_lid_t dlid)
        {
            struct info *p;

            if (cache == 0) {
                cache = (struct info *)ulm_malloc(sizeof(struct info) * nprocs());
                if (cache == 0) {
                    return false;
                }
                for (int i = 0; i < nprocs(); i++) {
                    cache[i].egress_port_index = 0;
                    cache[i].ah = VAPI_INVAL_HNDL;
                    cache[i].dlid = 0;
                    cache[i].next = 0;
                }
            }

            p = &(cache[dest]);
            while (p->next) { p = p->next; }
            if (p->ah == VAPI_INVAL_HNDL) {
                p->egress_port_index = pindex;
                p->ah = ah;
                p->dlid = dlid;
            }
            else {
                p->next = (struct info *)ulm_malloc(sizeof(struct info));
                if (p->next == 0) {
                    return false;
                }
                p = p->next;
                p->egress_port_index = pindex;
                p->ah = ah;
                p->dlid = dlid;
                p->next = 0;
            }
            
            return true;
        }

        VAPI_ret_t destroy(VAPI_hca_hndl_t hca_hndl) 
        {
            VAPI_ret_t vapi_result = VAPI_OK;
            struct info *p, *q;
            int i;
            
            if (cache == 0) {
                return vapi_result;
            }

            for (i = 0; i < nprocs(); i++) {
                p = &(cache[i]);
                if (p->ah != VAPI_INVAL_HNDL) {
                    vapi_result = VAPI_destroy_addr_hndl(hca_hndl, p->ah);
                    if (vapi_result != VAPI_OK) {
                        return vapi_result;
                    }
                    p->ah = VAPI_INVAL_HNDL;
                }
                q = p;
                p = p->next;
                q->next = 0;
                while (p) {
                    if (p->ah != VAPI_INVAL_HNDL) {
                        vapi_result = VAPI_destroy_addr_hndl(hca_hndl, p->ah);
                        if (vapi_result != VAPI_OK) {
                            return vapi_result;
                        }
                        p->ah = VAPI_INVAL_HNDL;
                    }
                    q = p;
                    p = p->next;
                    q->next = 0;
                    ulm_free(q);
                }
            }
            
            ulm_free(cache);
            return vapi_result; 
        }
};

/* no dynamic sizing to help performance -- or at least
 * not unintentionally hurt it... 
 */

/* basic record used for UD routing -- passed in bootstrap code */
typedef struct {
    /* UD QP number */
    VAPI_qp_num_t qp_num;
    /* destination LID -- no global routing header */
    IB_lid_t lid;
    /* destination LMC -- not used currently! */
    u_int8_t lmc;
    /* basic record flags -- invalid/invalid only currently! */
    u_int8_t flag;
} ib_ud_peer_info_t;

/* macros to set and examine flag of ib_ud_peer_info_t routing record */
#define PEER_INFO_IS_VALID(x) ((x.flag & 0x80) != 0)
#define SET_PEER_INFO_VALID(x) (x.flag |= 0x80)
#define SET_PEER_INFO_INVALID(x) (x.flag &= 0x7f)

/* information about remote processes with basic UD QP connectivity */
typedef struct {
    /* number of max. active HCAs, used as first "index" in info[] array */
    int max_hcas;
    /* number of max. active ports/HCA, used as second "index" in info[] array */
    int max_ports;
    /* number of entries per remote process in info[] array -- max_hcas * max_ports */
    int proc_entries;
    /* routing information for remote process */
    ib_ud_peer_info_t *info;
    /* nprocs()-sized array of integers -- to round-robin HCA usage of routing info */
    int *next_remote_hca;
    /* nprocs()-sized array of integers -- to round-robin port usage of routing info */
    int *next_remote_port;
} ib_ud_peer_t;

/* information about specific UD QP -- 1 per HCA */
typedef struct {
    VAPI_qp_hndl_t handle;
    VAPI_qp_prop_t prop;
    /* handle to single receive memory region of 2048 byte buffers */
    VAPI_mr_hndl_t recv_mr_handle;
    /* handle to single send memory region of 2048 byte buffers */
    VAPI_mr_hndl_t send_mr_handle;
    VAPI_mr_t recv_mr;
    VAPI_mr_t send_mr;
    /* flow-control tokens on send side to avoid send queue overflow */
    u_int32_t sq_tokens;
    /* flow-control tokens on send side to avoid recv queue overflow */
    u_int32_t rq_tokens;
    /* true if this UD QP receives multicast for this process */
    bool receive_multicast;
    /* size of receive MR in terms of 2048 byte buffers */
    int num_rbufs;
    /* size of send MR in terms of 2048 byte buffers */
    int num_sbufs;
    /* page-aligned beginning of recv MR */
    void *base_rbuf;
    /* page-aligned beginning of send MR */
    void *base_sbuf;
    /* cache of address handles */
    ib_ud_av_cache ah_cache;
} ib_ud_qp_state_t;

/* information associated with a specific HCA */
typedef struct {
    /* true if HCA handle retrieved from VIPKL */
    bool usable;
    /* number of ports in ACTIVE state */
    int num_active_ports;
    /* indices of active ports -- not port number */
    int active_ports[LAMPI_MAX_IB_HCA_PORTS];
    VAPI_hca_hndl_t handle;
    VAPI_hca_vendor_t vendor;
    VAPI_hca_cap_t cap;
    VAPI_hca_port_t ports[LAMPI_MAX_IB_HCA_PORTS];
    /* single protection domain per HCA */
    VAPI_pd_hndl_t pd;
    VAPI_cq_hndl_t recv_cq;
    VAPI_cq_hndl_t send_cq;
    /* size of receive completion queue */
    VAPI_cqe_num_t recv_cq_size;
    /* size of send completion queue */
    VAPI_cqe_num_t send_cq_size;
    /* tokens for each available send frag. desc. */
    unsigned int send_frag_avail;
    /* flow-control tokens to avoid recv CQ overflow */
    VAPI_cqe_num_t recv_cq_tokens;
    /* flow-control tokens to avoid send CQ overflow */
    VAPI_cqe_num_t send_cq_tokens;
    ib_ud_qp_state_t ud;
    /* receive fragment descriptors -- for UD comms */
    FreeListPrivate_t <ibRecvFragDesc> recv_frag_list;
    /* send fragment descriptors -- for UD comms */
    FreeListPrivate_t <ibSendFragDesc> send_frag_list;
    unsigned int ctlMsgsToSendFlag;
    unsigned int ctlMsgsToAckFlag;
    /* non-data control message tracking lists */
    ProcessPrivateMemDblLinkList ctlMsgsToSend[NUMBER_CTLMSGTYPES];
    ProcessPrivateMemDblLinkList ctlMsgsToAck[NUMBER_CTLMSGTYPES];
} ib_hca_state_t;

/* top-level IB information structure */
typedef struct {
    /* thread lock for all IB state access... */
    Locks lock;
    /* set if thread lock is held -- to prevent recursive locking only */
    bool locked;
    /* total number of HCAs */
    u_int32_t num_hcas;
    /* number of HCAs that have at least one port in the ACTIVE state */
    int num_active_hcas;
    /* used for round-robin allocation of HCAS for sends */
    int next_send_hca;
    /* used for round-robin allocation of ports for sends */
    int next_send_port;
    /* indices of HCAs that have at least one port in the ACTIVE state */
    int active_hcas[LAMPI_MAX_IB_HCAS];
    VAPI_hca_id_t hca_ids[LAMPI_MAX_IB_HCAS];
    ib_hca_state_t hca[LAMPI_MAX_IB_HCAS];
    /* common queue key for all QPs for this MPI job */
    VAPI_qkey_t qkey;
    /* multicast GID that we join... */
    VAPI_gid_t mcast_gid;
    ib_ud_peer_t ud_peers;
    /* number of maximum 2048 byte buffers for send and recv UD QP WQEs */
    int max_ud_2k_buffers;
    /* flow-control flag for fragment pacing of long send messages */
    int max_outst_frags;
    /* true if we do per-fragment standard ACKing */
    bool ack;
    /* true if we do some final memory checksumming */
    bool checksum;
} ib_state_t;

extern ib_state_t ib_state;

/* prototypes */
extern bool ib_register_chunk(int hca_index, void *addr, size_t size);

#endif
