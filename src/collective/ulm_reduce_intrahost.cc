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



#include <stdio.h>
#include <stdlib.h>

#include "internal/log.h"
#include "internal/options.h"
#include "internal/type_copy.h"
#include "os/atomic.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*!
 * ulm_reduce - intrahost function, shared memory logarithmic
 *              algorithm
 *
 * \param s_buf         Initial data
 * \param r_buf         Buffer to receive the reduced data
 * \param count         Number of objects
 * \param type          Data type of objects
 * \param op            Operation operation
 * \param root          The the root process
 * \param comm          The communicator ID
 * \return              ULM error code
 *
 * Description
 *
 * This function performs a reduce operation (such as sum, max,
 * logical and, etc.) across all processes in current communicator on
 * this host.
 *
 * The result of the reduction is returned at the process with rank
 * "root" if that process is on-host, or at the process with on-host
 * rank 0 otherwise.
 *
 * The reduction operation can be either one of a predefined list of
 * operations, or a user-defined operation.
 *
 * Algorithm
 *
 * This implementation uses a logarithmic fan in using a shared memory
 * buffer for collectives. Since the buffer is of fixed size, the
 * algorithm is applied blockwise.
 */
extern "C" int ulm_reduce_intrahost(const void *s_buf,
                                    void *r_buf,
                                    int count,
                                    ULMType_t *type,
                                    ULMOp_t *op, int root, int comm)
{
    CollectiveSMBuffer_t *smbuf;
    Communicator *communicator;
    Group *group;
    ULMFunc_t *func;
    int mask;
    int n;
    int nproc;
    int parity;
    int peer;
    int root_onhost;
    int root_host;
    int self_host;
    int self;
    int tag;
    size_t block_size;
    size_t offset;
    ssize_t block_count;
    unsigned char *rp;
    unsigned char *sp;
    unsigned char *smbuf_start;
    void *arg;
    void *peer_buf;
    void *self_buf;
    volatile int *peer_flag;
    volatile int *self_flag;

    /*
     * Fast return for trivial data
     */

    if (count == 0) {
        return ULM_SUCCESS;
    }

    if (type == NULL) {
        return ULM_ERR_BAD_PARAM;
    }

    /*
     * If only one process on host, increment tag, copy data and
     * return
     */

    communicator = communicators[comm];
    group = communicator->localGroup;
    nproc = group->onHostGroupSize;
    tag = communicator->get_base_tag(1);
    if (nproc == 1) {
        if (s_buf != MPI_IN_PLACE) {
            type_copy(r_buf, s_buf, count, type);
        }
        return ULM_SUCCESS;
    }

    /*
     * Pre-defined operations have a vector of function pointers
     * corresponding to the basic types.  User-defined operations have
     * a vector of length 1.
     */
    if (op->isbasic) {
        if (type->isbasic == 0) {
            ulm_err(("Error: ulm_reduce: "
                     "basic operation, non-basic datatype\n"));
            return ULM_ERR_BAD_PARAM;
        }
        func = op->func[type->op_index];
    } else {
        func = op->func[0];
    }

    /*
     * For fortran defined functions, pass a pointer to the fortran
     * type handle as the function argument, else pass a pointer to
     * the type struct.
     */
    if (op->fortran) {
        arg = (void *) &(type->fhandle);
    } else {
        arg = (void *) &type;
    }

    /*
     * Notation:
     *
     * nproc  - the number of procs on-host
     * self   - our on-host group procid (relative to the root
     *          process if the root is on this host)
     * peer   - current peer's on-host group procid (relative to the
     *          root process if the root is on this host)
     *
     * For all procs recursively pair processes with
     *   peer = self ^ 2^n
     * and if peer is less than nproc and send from the
     * superior to the inferior where it is accumulated.
     * Once a proc has done a send then we are done.
     */

    self = group->onHostProcID;
    self_host = group->hostIndexInGroup;
    root_host = group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];
    if (self_host == root_host) {
        root_onhost = group->mapGroupProcIDToOnHostProcID[root];
    } else {
        root_onhost =
            group->groupHostData[group->hostIndexInGroup].groupProcIDOnHost[0];
        root_onhost = group->mapGroupProcIDToOnHostProcID[root_onhost];
    }

    /*
     * Shift self relative to root
     */
    self = (self - root_onhost + nproc) % nproc;

#define REDUCE_DEBUG 0
#if REDUCE_DEBUG
#define DUMP_INT(X) fprintf(stderr, "    %s = %d\n", # X, X);
#define DUMP_ARRAY(X,SIZE) \
    fprintf(stderr, "    %s[] = ", # X); \
    for (int n = 0; n < SIZE; n++) { \
        fprintf(stderr, " %d", X[n]); \
    } \
    fprintf(stderr, "\n");

    fprintf(stderr, "ulm_reduce_intrahost\n");
    DUMP_INT(lampiState.global_rank);
    DUMP_INT(group->ProcID);
    DUMP_INT(lampiState.hostid);
    DUMP_INT(group->hostIndexInGroup);
    DUMP_INT(self_host);
    DUMP_INT(root);
    DUMP_INT(root_host);
    DUMP_INT(root_onhost);
    DUMP_INT(self);
    fflush(stderr);
    sleep(1);
#endif /* REDUCE_DEBUG */

    /*
     * Shared memory buffer set-up
     */

    smbuf = communicator->getCollectiveSMBuffer(tag, ULM_COLLECTIVE_REDUCE);

    /* initialize flags */
    if( smbuf->tag != tag ) {
       // lock and check
       smbuf->lock.lock();

       if( smbuf->tag != tag ) {
	  int *flag = (int *) smbuf->mem;
	  for (int i = 0; i < communicator->localGroup->onHostGroupSize; i++) {
	     flag[0] = flag[1] = -1;
	     flag += communicator->collectiveOpt.reduceOffset / sizeof(int);
	  }
	  mb();
	  smbuf->tag=tag;
       }

       /* unlock, and continue */
       smbuf->lock.unlock();
    }

    offset = communicator->collectiveOpt.reduceOffset;
    block_size = offset - 2 * sizeof(int);
    smbuf_start = (unsigned char *) smbuf->mem;
    self_flag = (int *) (smbuf_start + self * offset);
    self_buf = (void *) (self_flag + 2);

    /*
     * Apply the algorithm
     */

    block_count = block_size / type->extent;
    rp = (unsigned char *) r_buf;
    if (s_buf == MPI_IN_PLACE) {
        sp = rp;
    } else {
        sp = (unsigned char *) s_buf;
    }
    parity = 0;
    while (count > 0) {
        n = block_count < count ? block_count : count;
        type_copy(self_buf, sp, n, type);
        mb();
        for (mask = 1; mask < nproc; mask <<= 1) {
            peer = self ^ mask;
            peer_flag = (int *) (smbuf_start + peer * offset);
            if (peer >= nproc) {
                /* nothing this phase */ ;
            } else if (self < peer) {
                /* read from peer */
                self_flag[parity] = peer;
                mb();
                ULM_SPIN_AND_MAKE_PROGRESS(peer_flag[parity] != self);
                peer_buf = (void *) (peer_flag + 2);
                func(peer_buf, self_buf, &n, arg);
                self_flag[parity] = peer_flag[parity] = -1;
                mb();
            } else {
		/* signal peer that buffer is readable */
                self_flag[parity] = peer;
                mb();
		/* wait for peer to finish reading */
                ULM_SPIN_AND_MAKE_PROGRESS(self_flag[parity] == peer);
            }
            parity = parity ^ 1;
        }
        if (self == 0) {
            type_copy(rp, self_buf, n, type);
            mb();
        }
        count -= n;
        rp += n * type->extent;
        sp += n * type->extent;
    }

    /*
     * Release shared memory buffer
     */

    communicator->releaseCollectiveSMBuffer(smbuf);

    return ULM_SUCCESS;
}
