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
#include "internal/malloc.h"
#include "internal/options.h"
#include "internal/type_copy.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*!
 * ulm_reduce - interhost function, point to point logarithmic algorithm
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
 * logical and, etc.) across representative processes on each host in
 * a communicator, returning the result of the reduction at the
 * process with rank "root"
 *
 * The root process is the representative process on its host.
 *
 * Processes other than the representative processes return
 * immediately.
 *
 * The reduction operation can be either one of a predefined list of
 * operations, or a user-defined operation.
 *
 * Algorithm
 *
 * Use a logarithmic fan in across the hosts of the group.
 * logarithmic scaling).
 */
extern "C" int ulm_reduce_interhost(const void *s_buf,
                                    void *r_buf,
                                    int count,
                                    ULMType_t *type,
                                    ULMOp_t *op,
                                    int root,
                                    int comm)
{
    enum {
        REDUCE_SMALL_BUFSIZE = 2048
    };

    ULMFunc_t *func;
    ULMStatus_t status;
    Group *group;
    int mask, nhost, peer, peer_host, peer_host_r, rc;
    int root_host, self, self_host, self_host_r, tag;
    size_t bufsize;
    unsigned char small_buf[REDUCE_SMALL_BUFSIZE];
    void *arg;
    void *buf;

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
     * Algorithm: logarithmic fan in over hosts
     *
     * A representative process is used on each host, namely "root" on
     * the host with that process, and the lowest ranked process on
     * every other host.
     *
     * Host ranks shifted relative to root host:
     *   X_r = (X - root_host + nhost) % nhost
     *   X = (X_r + root_host) % nhost
     */

    if (s_buf != MPI_IN_PLACE) {
        type_copy(r_buf, s_buf, count, type);
    }

    group = communicators[comm]->localGroup;
    nhost = group->numberOfHostsInGroup;
    if (nhost == 1) {
        return ULM_SUCCESS;
    }
    tag = communicators[comm]->get_base_tag(1);
    self = group->ProcID;
    self_host = group->hostIndexInGroup;
    root_host = group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];
    if (self_host == root_host) {
        if (self != root) {
            return ULM_SUCCESS;                     /* not participating */
        }
    } else if (self != group->groupHostData[self_host].groupProcIDOnHost[0]) {
        return ULM_SUCCESS;                     /* not participating */
    }

    self_host_r = (self_host - root_host + nhost) % nhost;

#define REDUCE_DEBUG 0
#if REDUCE_DEBUG
#define DUMP_INT(X) fprintf(stderr, "    %s = %d\n", # X, X);
#define DUMP_ARRAY(X,SIZE) \
    fprintf(stderr, "    %s[] = ", # X); \
    for (int n = 0; n < SIZE; n++) { \
        fprintf(stderr, " %d", X[n]); \
    } \
    fprintf(stderr, "\n");

    fprintf(stderr, "ulm_reduce_interhost\n");
    DUMP_INT(lampiState.global_rank);
    DUMP_INT(group->ProcID);
    DUMP_INT(lampiState.hostid);
    DUMP_INT(group->hostIndexInGroup);
    DUMP_INT(self_host);
    DUMP_INT(root);
    DUMP_INT(root_host);
    fflush(stderr);
    sleep(1);
#endif /* REDUCE_DEBUG */

    rc = ULM_SUCCESS;
    for (mask = 1; mask < nhost; mask <<= 1) {
        peer_host_r = self_host_r ^ mask;
        if (peer_host_r >= nhost) {             /* nothing this time */
            continue;
        }
        peer_host = (peer_host_r + root_host) % nhost;
        if (peer_host == root_host) {
            peer = root;
        } else {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
        }
        if (self_host_r < peer_host_r) {        /* receive from peer */
            rc = ulm_recv(buf, count, type, peer, tag, comm, &status);
            if (rc != ULM_SUCCESS) {
                break;
            }
            func(buf, r_buf, &count, arg);
        } else {                                /* send to peer */
            rc = ulm_send(r_buf, count, type, peer, tag, comm,
                          ULM_SEND_STANDARD);
            break;
        }
    }

    if (bufsize > REDUCE_SMALL_BUFSIZE) {
        ulm_free(buf);
    }

    return rc;
}
