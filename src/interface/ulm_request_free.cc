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
#include "path/common/path.h"

/*
 * Free opaque ULMRequestHandle_t
 *
 * \param request	ULM request handle
 * \return		ULM return code
 */
extern "C" int ulm_request_free(ULMRequestHandle_t *request)
{
    bool incomplete;
    RecvDesc_t *recvDesc;
    BaseSendDesc_t *sendDesc;

    if (OPT_CHECK_API_ARGS) {
        // if request is NULL - return error
        if (*request == NULL) {
            return ULM_ERR_REQUEST;
        }
    }

    RequestDesc_t *tmpRequest = (RequestDesc_t *) (*request);
    Communicator *commPtr = communicators[tmpRequest->ctx_m];

    if (OPT_CHECK_API_ARGS) {
        // commPtr should still be valid...
        if (commPtr == 0) {
            return ULM_ERR_COMM;
        }
    }

    incomplete = ((tmpRequest->status == ULM_STATUS_INCOMPLETE) &&
                  (tmpRequest->messageDone==REQUEST_INCOMPLETE));

    if ((tmpRequest->requestType == REQUEST_TYPE_RECV) && incomplete) {
        // lock receive descriptor to doublecheck incomplete and prevent a thread 
        // race to avoid multiple frees or prevent this request from not being 
        // freed at all
		    recvDesc=(RecvDesc_t *)tmpRequest;
            if (usethreads())
                recvDesc->Lock.lock();
            incomplete = ((tmpRequest->status == ULM_STATUS_INCOMPLETE) &&
                  (!tmpRequest->messageDone));
            // set the freeCalled flag so that ulm_recv_request_free() will
            // be called by any of the various copy to app functions when they 
            // are done with this receive descriptor
            if (incomplete) {
                tmpRequest->freeCalled = true;
            }
            if (usethreads())
                recvDesc->Lock.unlock();
            // do nothing else at this time, if...
            if (tmpRequest->freeCalled) {
                // set request object to NULL
                *request = ULM_REQUEST_NULL;
                return ULM_SUCCESS;
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
    if (tmpRequest->requestType == REQUEST_TYPE_SEND) {
	    sendDesc=(BaseSendDesc_t *)tmpRequest;
        if (sendDesc->sendType == ULM_SEND_BUFFERED) {
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
    tmpRequest->status = ULM_STATUS_INVALID;

    if( tmpRequest->requestType == REQUEST_TYPE_SEND) {
	    /* 
	     * send - only mark this as complete, the progress
	     *   engine actually free's this object
	     */

	    /* check to see if send is complete - e.g. if all data
	     *   has been acked - request free is being called, 
	     *   so no need to check on this
	     */
	    BaseSendDesc_t *SendDesc=(BaseSendDesc_t *)tmpRequest;

	    /* note the the mpi request object has been freed */
	    SendDesc->freeCalled=1;

    } else {
    /* recv */
		recvDesc=(RecvDesc_t *)tmpRequest;
        // return request to free list
        if (usethreads()) {
            IrecvDescPool.returnElement(recvDesc);
        } else {
            IrecvDescPool.returnElementNoLock(recvDesc);
        }
    }

    // set request object to NULL
    *request = ULM_REQUEST_NULL;

    return ULM_SUCCESS;
}
