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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "queue/globals.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "internal/malloc.h"

/*!
 * ulm_allreduce_linear -- allreduce, linear algorithm
 *
 * \param s_buf       Initial data
 * \param r_buf       Buffer to receive the reduced data
 * \param count       Number of data objects
 * \param type        Data type (if non-null overrides object_size)
 * \param op          Operation structure
 * \param comm        Communicator
 * \return            ULM error code
 *
 * Description
 *
 * This function performs a global reduce operation (such as sum, max,
 * logical and, etc.) across all the members of the context specified
 * by comm, and returns the result of the reduction in all processes.
 *
 * The reduction operation can be either one of a predefined list of
 * operations, or a user-defined operation.
 *
 * Algorithm
 *
 * This slow algorithm is used when the function is not commutative.
 *
 * Operation order agrees with MPI spec: start with proc 0
 */
extern "C" int ulm_allreduce_linear(const void *s_buf,
                                    void *r_buf,
                                    int count,
                                    ULMType_t *type,
                                    ULMOp_t *op,
                                    int comm)
{
    enum {
        REDUCE_SMALL_BUFSIZE = 2048
    };
    ULMFunc_t *func;
    ULMStatus_t status;
    int nproc;
    int rc;
    int self;
    int tag;
    size_t bufsize;
    unsigned char small_buf[REDUCE_SMALL_BUFSIZE];
    void *arg;
    void *buf;

    ulm_dbg(("ulm_allreduce_linear\n"));

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
     * Pre-defined operations have a vector of function
     * pointers corresponding to the basic types.
     *
     * User-defined operations have a vector of length 1.
     */
    if (op->isbasic) {
        if (type == NULL || type->isbasic == 0) {
            ulm_err(("Error: ulm_allreduce: basic operation, non-basic datatype\n"));
            return ULM_ERROR;
        }
        func = op->func[type->op_index];
    } else {
        func = op->func[0];
    }

    /*
     * For fortran defined functions, pass a pointer to the fortran type
     * handle the function argument, else pass a pointer to the type
     * struct.
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
        buf = (void *) ulm_malloc(bufsize);
        if (buf == NULL) {
            return (ULM_ERR_OUT_OF_RESOURCE);
        }
    } else {
        buf = (void *) small_buf;
    }

    nproc = communicators[comm]->remoteGroup->groupSize;
    self = communicators[comm]->localGroup->ProcID;
    tag = communicators[comm]->get_base_tag(1);

    memmove(r_buf, s_buf, bufsize);

    rc = ULM_ERROR;

    /*
     * Get the result so far from my inferior
     */
    if (self > 0) {
        rc = ulm_recv(buf, count, type, self - 1, tag, comm, &status);
        if (rc != ULM_SUCCESS) {
            goto EXIT;
        }
        func(buf, r_buf, &count, arg);
    }

    /*
     * Send my result to my superior
     */
    if (self < nproc - 1) {
        rc = ulm_send(r_buf, count, type, self + 1, tag, comm, ULM_SEND_STANDARD);
        if (rc != ULM_SUCCESS) {
            goto EXIT;
        }
    }

    /*
     * Result at nproc - 1. Send the result to everyone else
     */
    rc = ulm_bcast(r_buf, count, type, nproc - 1, comm);
    if (rc != ULM_SUCCESS) {
        goto EXIT;
    }

    /*
     * Free resources and exit
     */

    rc = ULM_SUCCESS;
EXIT:
    if (bufsize > REDUCE_SMALL_BUFSIZE && buf) {
        ulm_free(buf);
    }
    return rc;
}
