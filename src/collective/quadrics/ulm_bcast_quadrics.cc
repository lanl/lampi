/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <stdlib.h>
#include <elan3/elan3.h>
#include "internal/options.h"
#include "internal/log.h"
#include "internal/linkage.h"
#include "internal/collective.h"
#include "internal/types.h"
#include "internal/type_copy.h"
#include "queue/globals.h"
#include "path/quadrics/state.h"

static int ulm_bcast_onhost(void *buf, int count, ULMType_t *type,
                            int root, int comm)
{
    Communicator *comm_ptr;
    Group *group;
    CollectiveSMBuffer_t *coll_desc;
    int rc;
    int self;
    long long tag;
    size_t bcast_size = 0;
    size_t copy_buffer_size = 0;
    size_t shared_buffer_size = 0;
    size_t offset = 0;
    size_t ti = 0;
    size_t mi = 0;
    size_t mo = 0;
    unsigned char *p;
    void *RESTRICT_MACRO shared_buffer;

    group = communicators[comm]->localGroup;
    bcast_size = count * type->extent;
    comm_ptr = (Communicator *) communicators[comm];
    bcast_size = count * type->extent;

    rc = ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));

    /* set up collective descriptor, shared buffer */
    tag = comm_ptr->get_base_tag(1);
    coll_desc = comm_ptr->getCollectiveSMBuffer(tag);
    shared_buffer = coll_desc->mem;
    shared_buffer_size = (size_t) coll_desc->max_length;
    copy_buffer_size = (bcast_size < shared_buffer_size) ? bcast_size 
        : shared_buffer_size;

    while (ti < (size_t) count) {
        offset = 0;
        if (self == root) {
            type_pack(TYPE_PACK_PACK, shared_buffer, copy_buffer_size,
                      &offset, buf, count, type, &ti, &mi, &mo);
            wmb();
            coll_desc->flag = 2;
        } else {
            ULM_SPIN_AND_MAKE_PROGRESS(coll_desc->flag != 2);
            type_pack(TYPE_PACK_UNPACK, shared_buffer, copy_buffer_size,
                      &offset, buf, count, type, &ti, &mi, &mo);
        }

        if (ti != (size_t) count) {
            comm_ptr->smpBarrier(comm_ptr->barrierData);
            coll_desc->flag = 1;
            comm_ptr->smpBarrier(comm_ptr->barrierData);
        }
    }

    comm_ptr->releaseCollectiveSMBuffer(coll_desc);
    return ULM_SUCCESS;
}

CDECL
int ulm_bcast_quadrics(void *buf, int count, ULMType_t * type,
                       int root, int comm)
{
    int self;
    int nhosts;
    long long tag;
    int rc;
    ULMStatus_t status;
    size_t mcast_buf_sz = 0;
    int myVp, vpid;
    int root_send_proc, my_send_proc;
    unsigned char *mcast_buf;
    ELAN_LOCATION loc;
    int in_stripe = 0;
    int root_host, my_host;
    Group *grp;
    int i;

    ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));
    ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS, &nhosts, sizeof(int));
    
    if (nhosts == 1) { /* one-host case */
        return ulm_bcast_onhost(buf, count, type, root, comm);        
    }

    /* 
     * make sure procs are contiguous.  Also, when called for
     * the first time, getMcastVpid/getMcastBuf must be called by 
     * everyone in the communicator.
     */
    rc = communicators[comm]->getMcastVpid(0, &vpid);
    if (rc != ULM_SUCCESS || vpid < 0) {
        ulm_err(("Error: comm=%d, proc=%d returned vpid=%d, rc=%d\n",
                 comm, self, vpid, rc));
        goto DEFAULT;
    }
    mcast_buf = (unsigned char *) 
        communicators[comm]->getMcastBuf(0, &mcast_buf_sz);
    if (mcast_buf == 0) {
        ulm_err(("Error: comm=%d, proc=%d couldn't fetch mcast buf\n",
                 comm, self));
        goto DEFAULT;
    }

    /* make sure packed message will fit within the multicast
     * buffer */
    if (count * type->packed_size > mcast_buf_sz) {
        goto DEFAULT;
    }

    /* see if this proc is in the hw multicast stripe */
    grp = communicators[comm]->localGroup;
    myVp = grp->mapGroupProcIDToGlobalProcID[grp->ProcID];
    loc = elan3_vp2location(myVp, &quadricsCap);
    if (loc.Context == communicators[comm]->hw_ctx_stripe) {
        in_stripe = 1;
    }

    /* determine sending process on root host, my host */
    root_host = grp->mapGroupProcIDToHostID[root];
    my_host = grp->mapGroupProcIDToHostID[self];
    root_send_proc = root;
    my_send_proc = self;
    for (i = 0; i < grp->groupHostData[root_host].nGroupProcIDOnHost; i++) {
        vpid = grp->mapGroupProcIDToGlobalProcID[grp->groupHostData[root_host]
                                                 .groupProcIDOnHost[i]];
        loc = elan3_vp2location(vpid, &quadricsCap);
        if (loc.Context == communicators[comm]->hw_ctx_stripe) {
            root_send_proc = grp->groupHostData[root_host].groupProcIDOnHost[i];
            break;
        }
    }
    for (i = 0; i < grp->groupHostData[my_host].nGroupProcIDOnHost; i++) {
        vpid = grp->mapGroupProcIDToGlobalProcID[grp->groupHostData[my_host]
                                                 .groupProcIDOnHost[i]];
        loc = elan3_vp2location(vpid, &quadricsCap);
        if (loc.Context == communicators[comm]->hw_ctx_stripe) {
            my_send_proc = grp->groupHostData[my_host].groupProcIDOnHost[i];
            break;
        }
    }

    /* broadcast from root to all on root's host */    
    tag = communicators[comm]->get_base_tag(1);
    if (my_host == root_host) {
        rc = ulm_bcast_onhost(buf, count, type, root, comm);
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: in multicast send, returned %d\n", rc));
            return rc;
        }
        if (in_stripe) {
            rc = ulm_send(buf, count, type, -1, tag, comm, ULM_SEND_MULTICAST);
            if (rc != ULM_SUCCESS) {
                ulm_err(("Error: in multicast send, returned %d\n", rc));
                return rc;
            }
            /* Vacuous recv forced by quadrics hw multicast */
            rc = ulm_recv(buf, count, type, root_send_proc, tag, comm, &status);
            if (rc != ULM_SUCCESS) {
                ulm_err(("Error: in multicast recv, returned %d\n", rc));
                return rc;
            }
        }
    } else {
        if (in_stripe) {
            rc = ulm_recv(buf, count, type, root_send_proc, tag, comm, &status);
            if (rc != ULM_SUCCESS) {
                ulm_err(("Error: in multicast recv, returned %d\n", rc));
                return rc;
            }
        }
        rc = ulm_bcast_onhost(buf, count, type, my_send_proc, comm);
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: in multicast recv, returned %d\n", rc));
            return rc;
        }
    }

    return ULM_SUCCESS;

DEFAULT:
    return ulm_bcast(buf, count, type, root, comm);
}
