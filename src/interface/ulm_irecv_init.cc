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

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
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
extern "C" int ulm_irecv_init(void *buf, size_t size, ULMType_t *dtype,
                              int sourceProc, int tag, int comm,
                              ULMRequestHandle_t *request, int persistent)
{
    int errorCode;
    int listIndex = 0;
    RequestDesc_t *ulmRequest;
    RecvDesc_t *RecvDescriptor;

    if (usethreads()) {
        RecvDescriptor = IrecvDescPool.getElement(0, errorCode);
        if (errorCode != ULM_SUCCESS)
            return errorCode;
    } else {
        RecvDescriptor = IrecvDescPool.getElementNoLock(0, errorCode);
        if (errorCode != ULM_SUCCESS)
            return errorCode;
    }
    ulmRequest=(RequestDesc_t *)RecvDescriptor;

    // set done flag to false
    ulmRequest->messageDone = false;

    ulmRequest->WhichQueue = REQUESTINUSE;

    // set value of pointer
    *request = (ULMRequestHandle_t) ulmRequest;

    // set message type in ReturnHandle
    ulmRequest->requestType = REQUEST_TYPE_RECV;

    // set data type
    ulmRequest->datatype = dtype;

    // set receive address
    if ((dtype != NULL) && (dtype->layout == CONTIGUOUS) && (dtype->num_pairs != 0)) {
        ulmRequest->pointerToData = (void *)((char *)buf + dtype->type_map[0].offset);
    }
    else {
        ulmRequest->pointerToData = buf;
    }

    // set destination process (need to check for completion)
    ulmRequest->posted_m.proc.source_m = sourceProc;

    // set communicator
    ulmRequest->ctx_m = comm;

    // set tag
    ulmRequest->posted_m.UserTag_m = tag;

    // set posted length
    //
    if (dtype == NULL) {
        ulmRequest->posted_m.length_m = size;
    } else {
        ulmRequest->posted_m.length_m = dtype->packed_size * size;
    }

    // set request state to inactive
    ulmRequest->status = ULM_STATUS_INITED;

    // set the persistence flag
    if (persistent) {
        ulmRequest->persistent = true;
        // increment requestRefCount
        communicators[comm]->refCounLock.lock();
        (communicators[comm]->requestRefCount)++;
        communicators[comm]->refCounLock.unlock();
    } else {
        ulmRequest->persistent = false;
    }

    return ULM_SUCCESS;
}
