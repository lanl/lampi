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

#include "client/ULMClient.h"
#include "internal/buffer.h"
#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "path/common/path.h"
#include "queue/globals.h"
#include "ulm/ulm.h"

/*
 * Free opaque ULMRequest_t
 *
 * \param request	ULM request handle
 * \return		ULM return code
 */
extern "C" int ulm_request_free(ULMRequest_t *request)
{
    RequestDesc_t *RequestDesc = (RequestDesc_t *) (*request);
    RecvDesc_t *RecvDesc = (RecvDesc_t *) RequestDesc;
    SendDesc_t *SendDesc = (SendDesc_t *) RequestDesc;
    Communicator *commPtr = communicators[RequestDesc->ctx_m];
    bool incomplete;

    if (OPT_CHECK_API_ARGS) {
        if (*request == NULL) {
            return ULM_ERR_REQUEST;
        }
        if (commPtr == 0) {
            return ULM_ERR_COMM;
        }
    }

    incomplete = ((RequestDesc->status == ULM_STATUS_INCOMPLETE) &&
                  (RequestDesc->messageDone == REQUEST_INCOMPLETE));

    if ((RequestDesc->requestType == REQUEST_TYPE_RECV) && incomplete) {
        // lock receive descriptor to doublecheck incomplete and prevent a thread
        // race to avoid multiple frees or prevent this request from not being
        // freed at all
        if (usethreads())
            RecvDesc->Lock.lock();
        incomplete = ((RequestDesc->status == ULM_STATUS_INCOMPLETE) &&
                      (!RequestDesc->messageDone));
        // set the freeCalled flag so that ulm_recv_request_free() will
        // be called by any of the various copy to app functions when they
        // are done with this receive descriptor
        if (incomplete) {
            RequestDesc->freeCalled = true;
        }
        if (usethreads())
            RecvDesc->Lock.unlock();
        // do nothing else at this time, if...
        if (RequestDesc->freeCalled) {
            // set request object to NULL
            *request = ULM_REQUEST_NULL;
            return ULM_SUCCESS;
        }
    }

    if (RequestDesc->persistent) {
        // decrement request reference count
        commPtr->refCounLock.lock();
        (commPtr->requestRefCount)--;
        commPtr->refCounLock.unlock();
        // datatype ref counts are decremented in CheckforAckedMessages()
    }

    // all buffered send requests must free their allocations

    if (RequestDesc->requestType == REQUEST_TYPE_SEND) {
        if (SendDesc->sendType == ULM_SEND_BUFFERED) {
            ULMBufferRange_t *allocation;
            if (usethreads())
                lock(&(lampiState.bsendData->Lock));
            allocation = ulm_bsend_find_alloc((ssize_t) (-1), *request);
            if (allocation != NULL) {
                if (allocation->refCount == -1)
                    allocation->refCount = 0;
                allocation->freeAtZeroRefCount = 1;
                if (allocation->refCount == 0)
                    ulm_bsend_clean_alloc(0);
            }
            if (usethreads())
                unlock(&(lampiState.bsendData->Lock));
        }
    }

    // reset which list

    RequestDesc->status = ULM_STATUS_INVALID;

    if (RequestDesc->requestType == REQUEST_TYPE_SEND) {
        // note the the mpi request object has been freed */
        SendDesc->freeCalled = 1;
    } else {
        // return request to free list
        if (usethreads()) {
            IrecvDescPool.returnElement(RecvDesc);
        } else {
            IrecvDescPool.returnElementNoLock(RecvDesc);
        }
    }

    *request = ULM_REQUEST_NULL;

    return ULM_SUCCESS;
}
