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

#include <stdio.h>

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "os/atomic.h"
#include "ulm/ulm.h"
#include "internal/log.h"

/*!
 * Allocate and initialize a high level irecv descriptor.
 *
 * \param sourceProc    ProcID of the source process
 * \param comm          ID of communicator
 * \param tag           Tag for matching
 * \param data          Buffer for received data
 * \param length        Size of buffer in bytes
 * \param request       ULM request handle
 * \param persistent     flag indicating if the request is persistent
 * \return              ULM return code
 *
 * The descriptor is allocated from the library's internal pools and
 * then filled in.  A request object is also allocated.
 */
extern "C" int ulm_irecv_init(void *buf,
                              size_t size,
                              ULMType_t *dtype,
                              int sourceProc,
                              int tag,
                              int comm,
                              ULMRequest_t *request,
                              int persistent)
{
    RecvDesc_t *RecvDesc;
    int rc;

    // get a receive request descriptor

    if (usethreads()) {
        RecvDesc = IrecvDescPool.getElement(0, rc);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
    } else {
        RecvDesc = IrecvDescPool.getElementNoLock(0, rc);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
    }

    // initialize descriptor

    RecvDesc->WhichQueue = REQUESTINUSE;
    RecvDesc->requestType = REQUEST_TYPE_RECV;
    RecvDesc->messageDone = REQUEST_INCOMPLETE; // used by test/wait
    RecvDesc->freeCalled = false;               // ulm_free_request() called
    RecvDesc->status = ULM_STATUS_INITED;
    RecvDesc->posted_m.peer_m = sourceProc;
    RecvDesc->ctx_m = comm;
    RecvDesc->posted_m.tag_m = tag;
    RecvDesc->datatype = dtype;
    if ((dtype != NULL) &&
        (dtype->layout == CONTIGUOUS) &&
        (dtype->num_pairs != 0)) {
        RecvDesc->addr_m = (void *) ((char *) buf +
                                      dtype->type_map[0].offset);
    } else {
        RecvDesc->addr_m = buf;
    }
    if (dtype == NULL) {
        RecvDesc->posted_m.length_m = size;
    } else {
        RecvDesc->posted_m.length_m = dtype->packed_size * size;
    }

    // persistent request?

    if (persistent) {
        RecvDesc->persistent = true;
        // increment requestRefCount
        communicators[comm]->refCounLock.lock();
        (communicators[comm]->requestRefCount)++;
        communicators[comm]->refCounLock.unlock();
        // increment datatype reference count
        ulm_type_retain(RecvDesc->datatype);
    } else {
        RecvDesc->persistent = false;
    }

    *request = (ULMRequest_t) RecvDesc;

    return ULM_SUCCESS;
}
