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

#include "client/ULMClient.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "queue/globals.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

/*!
 * initialize send descriptor
 * \param buf           Data buffer
 * \param size          Buffer size in bytes if dtype is NULL,
 *                      else number of buftype descriptors in buf
 * \param dtype         Datatype descriptor
 * \param dest          Destination process
 * \param tag           Message tag
 * \param comm          Communicator ID
 * \param request       ULM request
 * \param sendMode      type of send
 * \param persistent     flag indicating if the request is persistent
 * \return              ULM return code
 *
 * The descriptor is allocated from the library's internal pools and
 * then filled in.  A request object is also allocated.
 */
extern "C" int ulm_isend_init(void *buf,
                              size_t size,
                              ULMType_t *dtype,
                              int dest,
                              int tag,
                              int comm,
                              ULMRequest_t *request,
                              int sendMode,
                              int persistent)
{
    SendDesc_t *SendDesc;
    int rc;
    void *ptr;

    // get a send descriptor by binding to a given path (this can fail)

    rc = communicators[comm]->pt2ptPathSelectionFunction(&ptr, comm, dest);
    if (rc != ULM_SUCCESS) {
        return rc;
    }
    SendDesc = (SendDesc_t *) ptr;

    assert(SendDesc->WhichQueue == SENDDESCFREELIST);

    // initialize descriptor

    SendDesc->WhichQueue = REQUESTINUSE;
    SendDesc->requestType = REQUEST_TYPE_SEND;
    SendDesc->messageDone = REQUEST_INCOMPLETE; // used by test/wait
    SendDesc->datatype = dtype;
    SendDesc->posted_m.peer_m = dest;
    SendDesc->posted_m.tag_m = tag;
    SendDesc->ctx_m = comm;
    SendDesc->status = ULM_STATUS_INITED;
    SendDesc->sendType = sendMode;
    if (dtype == NULL) {
        SendDesc->posted_m.length_m = size;
    } else {
        SendDesc->posted_m.length_m = dtype->packed_size * size;
    }
    if ((dtype != NULL) &&
        (dtype->layout == CONTIGUOUS) &&
        (dtype->num_pairs != 0)) {
        SendDesc->addr_m = (void *) ((char *) buf +
                                      dtype->type_map[0].offset);
    } else {
        SendDesc->addr_m = buf;
    }

    // persistent request?

    if (persistent) {
        SendDesc->persistent = true;
        // increment requestRefCount
        communicators[comm]->refCounLock.lock();
        (communicators[comm]->requestRefCount)++;
        communicators[comm]->refCounLock.unlock();
        // increment datatype reference counts
        ulm_type_retain(SendDesc->datatype);
        ulm_type_retain(SendDesc->bsendDatatype);
    } else {
        SendDesc->persistent = false;
    }

    *request = (ULMRequest_t) SendDesc;

    return ULM_SUCCESS;
}
