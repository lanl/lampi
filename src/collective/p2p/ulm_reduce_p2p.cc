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
#include "ulm/ulm.h"
#include "queue/globals.h"
#include "internal/malloc.h"

/*!
 * ulm_reduce - reduce function, point to point logarithmic algorithm
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
 * logical and, etc.) across all the members of the group specified by
 * the context ID, and returns the result of the reduction at the
 * process with rank "root"
 *
 * The reduction operation can be either one of a predefined list of
 * operations, or a user-defined operation.
 *
 * Algorithm
 *
 * Use isend/irecv across the  group recursively (and  hence achieve
 * logarithmic scaling).
 *
 * No account is taken of the topology of the network with respect to
 * the group.  This guarantees that the same result will be obtained no
 * matter how the processes in the group are distributed across the
 * system.
 *
 * This conforms to the recommendations of the MPI specification.
 */
extern "C" int ulm_reduce_p2p(const void *s_buf,
                              void *r_buf,
                              int count,
                              ULMType_t * type,
                              ULMOp_t * op, int root, int comm)
{
    enum {
        REDUCE_SMALL_BUFSIZE = 2048
    };

    ULMFunc_t *func;
    ULMStatus_t status;
    int mask;
    int nproc;
    int peer;
    int peer_r;
    int rc;
    int self;
    int self_r;
    int tag;
    size_t bufsize;
    unsigned char small_buf[REDUCE_SMALL_BUFSIZE];
    void *arg;
    void *buf;

    ulm_dbg(("ulm_reduce_p2p\n"));

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
     * Temporary buffer for receiving incremental data
     */
    bufsize = count * type->extent;
    if (bufsize > REDUCE_SMALL_BUFSIZE) {
        buf = ulm_malloc(bufsize);
        if (buf == NULL) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }
    } else {
        buf = (void *) small_buf;
    }

    /*
     * Algorithm
     *
     * Notation:
     *
     * nproc  - the number of procs
     * self   - our group procid
     * peer   - current peer's group procid
     * self_r - our group procid relative to the root
     * peer_r - current peer's group procid relative to the root
     *
     * For all procs recursively pair processes with
     *   peer_r = self_r ^ 2^n
     * and if peer_r is less than nproc and send from the
     * superior to the inferior where it is accumulated.
     * Once a proc has done a send then we are done.
     */

    if (s_buf != MPI_IN_PLACE) {
        type_copy(r_buf, s_buf, count, type);
    }

    nproc = communicators[comm]->localGroup->groupSize;
    if (nproc == 1) {
        return ULM_SUCCESS;
    }
    self = communicators[comm]->localGroup->ProcID;
    self_r = (self - root + nproc) % nproc;
    tag = communicators[comm]->get_base_tag(1);

    rc = ULM_SUCCESS;
    for (mask = 1; mask < nproc; mask <<= 1) {
        peer_r = self_r ^ mask;
        peer = (peer_r + root) % nproc;
        if (peer_r >= nproc) {
            continue;
        } else if (self_r < peer_r) {
            rc = ulm_recv(buf, count, type, peer, tag, comm, &status);
            if (rc != ULM_SUCCESS) {
                break;
            }
            func(buf, r_buf, &count, arg);
        } else {
            rc = ulm_send(r_buf, count, type, peer,
                          tag, comm, ULM_SEND_STANDARD);
            break;
        }
    }

    if (bufsize > REDUCE_SMALL_BUFSIZE) {
        ulm_free(buf);
    }

    return rc;
}
