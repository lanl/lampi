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
#include "internal/buffer.h"
#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "ulm/ulm.h"

/*
 * Free opaque ULMRequestHandle_t
 *
 * \param request	ULM request handle
 * \return		ULM return code
 */
extern "C" int ulm_request_free(ULMRequestHandle_t *request)
{
    bool incomplete;

    if (OPT_CHECK_API_ARGS) {
        // if request is NULL - return error
        if (*request == NULL) {
            return ULM_ERR_REQUEST;
        }
    }

    RequestDesc_t *tmpRequest = (RequestDesc_t *) (*request);
    Communicator *commPtr = communicators[tmpRequest->ctx_m];

    if (OPT_CHECK_API_ARGS) {
        // sanity check
        assert(tmpRequest->WhichQueue == REQUESTINUSE);
        // commPtr should still be valid...
        if (commPtr == 0) {
            return ULM_ERR_COMM;
        }
    }
    // decrement request reference count
    if (tmpRequest->persistent) {
        commPtr->refCounLock.lock();
        (commPtr->requestRefCount)--;
        commPtr->refCounLock.unlock();
    }
    // all buffered send requests must have their allocations
    // freed
    if ((tmpRequest->requestType == REQUEST_TYPE_SEND) &&
        (tmpRequest->sendType == ULM_SEND_BUFFERED)) {
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
    // if request is started and incomplete, store on
    // incomplete request object list (until messageDone becomes true); otherwise
    // store on freelist 0 (it's safe to be reallocated)
    incomplete = ((tmpRequest->status == ULM_STATUS_INCOMPLETE) &&
                  (!tmpRequest->messageDone));

    // reset which list
    tmpRequest->WhichQueue = REQUESTFREELIST;
    tmpRequest->status = ULM_STATUS_INVALID;

    if (incomplete) {
        if (usethreads())
            _ulm_incompleteRequests.Append(tmpRequest);
        else
            _ulm_incompleteRequests.AppendNoLock(tmpRequest);
    } else {
        // return request to free list
        if (usethreads()) {
            ulmRequestDescPool.returnElement(tmpRequest);
        } else {
            ulmRequestDescPool.returnElementNoLock(tmpRequest);
        }
    }

    // set request object to NULL
    *request = ULM_REQUEST_NULL;

    return ULM_SUCCESS;
}
