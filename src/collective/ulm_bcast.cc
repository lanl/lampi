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



#include "internal/log.h"
#include "internal/options.h"
#include "internal/type_copy.h"
#include "ulm/ulm.h"
#include "queue/globals.h"
#include "collective/coll_fns.h"

/*!
 * ulm_bcast - broadcast function entry point
 *
 * \param buf           (choice)    The starting address of the send buffer
 * \param count         (int)       Number of entries in the buffer
 * \param type          (handle)    Data type of send buffer elements
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 *
 * This top-level entry point selects the appropriate algorithm.
 *
 * The algorithm is called blockwise to moderate the load for very
 * large data sets.
 */

enum {
    KILOBYTE = 1 << 10,
    MEGABYTE = 1 << 20,
    BLOCK_SIZE = 128 * KILOBYTE
};

extern "C" int ulm_bcast(void *buf, int count, ULMType_t *type, int root, int comm)
{
    Communicator *comm_ptr;
    Group *group;
    CollectiveSMBuffer_t *coll_desc;
    //extern ulm_bcast_t ulm_bcast_interhost;
    int block_count = 0;
    int comm_root;
    int n;
    int rc;
    int self;
    int total_hosts;
    int total_procs;
    int cnt;
    int hi;
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
    hi = group->hostIndexInGroup;
    comm_root = group->groupHostData[hi].groupProcIDOnHost[0];

    rc = ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));


    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS, &total_hosts, sizeof(int));

    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_PROCS, &total_procs, sizeof(int));

    /* set up collective descriptor, shared buffer */
    tag = comm_ptr->get_base_tag(1);
    coll_desc = comm_ptr->getCollectiveSMBuffer(tag);
    shared_buffer = coll_desc->mem;
    shared_buffer_size = (size_t) coll_desc->max_length;
    copy_buffer_size = (bcast_size < shared_buffer_size) ? bcast_size : shared_buffer_size;
    /* single-host case */
    if (total_hosts == 1) {
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

    /* more than 1 host, so send data around to comm-roots
     * using ulm_bcast_interhost */
    block_count = BLOCK_SIZE / type->packed_size;
    if (block_count == 0) {
        block_count = 1;
    }
    p = (unsigned char *) buf;
    cnt = count;
    while (cnt > 0) {
        n = (block_count < cnt) ? block_count : cnt;
        rc = ulm_bcast_interhost(p, n, type, root, comm);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
        cnt -= n;
        p += n * type->extent;
    }

    /* if one proc per host, we're done, otherwise, we barrier
     * to allow the non comm-root procs to catch up */
    if (total_procs == total_hosts) {
        comm_ptr->releaseCollectiveSMBuffer(coll_desc);
        return ULM_SUCCESS;
    } else {
        comm_ptr->smpBarrier(comm_ptr->barrierData);
    }

    /* disperse data on-box */
    if (group->mapGroupProcIDToHostID[root] == group->mapGroupProcIDToHostID[self]) {
        comm_root = root;
    }

    while (ti < (size_t) count) {
        offset = 0;
        if (self == comm_root) {
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

#ifdef USE_ELAN_COLL
extern "C" int ulm_bcast_quadrics(void *buf, int count, ULMType_t *type, int root, int comm)
{
    Communicator *comm_ptr;
    Group *group;
    CollectiveSMBuffer_t *coll_desc;
    int block_count = 0;
    int comm_root;
    int n;
    int rc;
    int self;
    int total_hosts;
    int total_procs;
    int cnt;
    int hi;
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

    START_MARK;

    if (!communicators[comm]->hw_bcast_enabled)
    {
        END_MARK;
        return rc = ulm_bcast(buf, count, type, root, comm);
    }

    group = communicators[comm]->localGroup;
    bcast_size = count * type->extent;
    comm_ptr = (Communicator *) communicators[comm];
    bcast_size = count * type->extent;
    hi = group->hostIndexInGroup;
    comm_root = group->groupHostData[hi].groupProcIDOnHost[0];

    rc = ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS,
                      &total_hosts, sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_PROCS,
                      &total_procs, sizeof(int));

    /* set up collective descriptor, shared buffer */
    tag = comm_ptr->get_base_tag(1);
    coll_desc = comm_ptr->getCollectiveSMBuffer(tag);
    shared_buffer = coll_desc->mem;
    shared_buffer_size = (size_t) coll_desc->max_length;
    copy_buffer_size = (bcast_size < shared_buffer_size) ?
bcast_size : shared_buffer_size;

    /* single-host case */
    if (total_hosts == 1) {
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

    /* more than 1 host, so send data around to comm-roots
        * using ulm_bcast_interhost */

    if (group->mapGroupProcIDToHostID[root] ==
        group->mapGroupProcIDToHostID[self]) {
        comm_root = root;
    }

    block_count = BLOCK_SIZE / type->packed_size;
    if (block_count == 0) {
        block_count = 1;
    }
    p = (unsigned char *) buf;
    cnt = count;

    while (cnt > 0) {

        /* Consider using a pool of descriptors and reuse them */
        bcast_request_t * bcast_desc= comm_ptr->bcast_queue.first_free;

        n = (block_count < cnt) ? block_count : cnt;
        tag = comm_ptr->get_base_tag(1);

        /* Initialize the request, and binds to a channel */
        rc = comm_ptr->bcast_bind(bcast_desc, self, root, comm_root,
                                  p, n, type, tag, comm, coll_desc, ULM_SEND_STANDARD);

        /* If not successful, switch to ulm_bcast */
        if (rc)
        {
            rc = ulm_bcast(p, n, type, root, comm);
            goto next_block;
        }

        /* Commit the request at successful binding */
        comm_ptr->commit_bcast_req(bcast_desc);

        /* Have all processes to start the bcast,
            * in reality, only the sender triggers broadcast DMA */
        comm_ptr->ibcast_start(bcast_desc);

        /*
            * Wait for the bcast to complete,
         * Data dispersion across a SMP box is also done within
         */
        rc = comm_ptr->bcast_wait(bcast_desc);

#ifdef  RELIABILITY_ON
        /* Fail-over if not completed. Incorrect implementation */
        if ( rc )
        {
            comm_ptr->free_bcast_req(bcast_desc);
            rc = ulm_bcast(p, n, type, root, comm);
        }
#endif

next_block:
            if (rc != ULM_SUCCESS)
            {
                comm_ptr->releaseCollectiveSMBuffer(coll_desc);
                return rc;
            }

        cnt -= n;
        p += n * type->extent;
    }

    /* Release the SMBuffer, done in bcast_wait */
    comm_ptr->releaseCollectiveSMBuffer(coll_desc);

    END_MARK;
    return ULM_SUCCESS;
}

#endif
