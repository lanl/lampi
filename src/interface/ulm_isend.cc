/*
 * Copyright 2002-2003. The Regents of the University of
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "queue/globals.h"

/*!
 * non blocking send
 *
 * \param buf           Data buffer
 * \param buf_size      Buffer size, in bytes, if dtype is NULL, or the
 *                      number of ULMType_t's in buf, if dtype is not NULL
 * \param dtype         Datatype descriptor for non-contiguous data,
 *                      NULL if data is contiguous
 * \param dest          Destination process
 * \param tag           Message tag
 * \param comm          Communicator ID
 * \param request       ULM request
 * \param sendMode      Send mode - standard, buffered, synchronous, or ready
 * \return              ULM return code
 */
extern "C" int ulm_isend(void *buf,
                         size_t size,
                         ULMType_t *dtype,
                         int dest,
                         int tag,
                         int comm,
                         ULMRequest_t *request,
                         int sendMode)
{
    SendDesc_t *SendDesc;
    void *ptr;
    int rc;

    /* bind send descriptor to a given path....this can fail... */
    rc = communicators[comm]->pt2ptPathSelectionFunction(&ptr, comm, dest);
    if (rc != ULM_SUCCESS) {
        return rc;
    }

    SendDesc = (SendDesc_t *) ptr;

    // initialize descriptor

    SendDesc->WhichQueue = REQUESTINUSE;
    SendDesc->messageDone = REQUEST_INCOMPLETE; // used by test/wait
    SendDesc->requestType = REQUEST_TYPE_SEND;
    SendDesc->datatype = dtype;
    SendDesc->posted_m.peer_m = dest;
    SendDesc->posted_m.tag_m = tag;
    SendDesc->ctx_m = comm;
    SendDesc->status = ULM_STATUS_INITED;
    SendDesc->sendType = sendMode;

    // set buf type, posted size, and set send address
    if (dtype == NULL) {
        SendDesc->posted_m.length_m = size;
    } else {
        SendDesc->posted_m.length_m = dtype->packed_size * size;
    }
    if ((dtype != NULL) && (dtype->layout == CONTIGUOUS)
        && (dtype->num_pairs != 0)) {
        SendDesc->addr_m =
            (void *) ((char *) buf + dtype->type_map[0].offset);
    } else {
        SendDesc->addr_m = buf;
    }

    // set the persistence flag
    SendDesc->persistent = false;

    *request = (ULMRequest_t) SendDesc;

    // start send
    rc = communicators[comm]->isend_start(&SendDesc);
    if (rc != ULM_SUCCESS) {
        ulm_request_free((ULMRequest_t *) request);
        return rc;
    }

    // update request object - invalid; needs to happen in
    // isend_start, since ulmRequest may actually change in
    // isend_start
    SendDesc->status = ULM_STATUS_INCOMPLETE;

    return ULM_SUCCESS;
}
