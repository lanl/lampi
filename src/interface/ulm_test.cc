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

#include "internal/profiler.h"
#include "internal/options.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"
#include "internal/buffer.h"

/*!
 * Test posted request for completion without blocking
 *
 * \param request       Request to test for completion
 * \param completed     Flag to set if request is complete
 * \param status        Status struct to be filled in on completion
 * \return              ULM return code
 */
extern "C" int ulm_test(ULMRequestHandle_t *request, int *completed,
                        ULMStatus_t *status)
{
    RequestDesc_t *tmpRequest;
    BaseSendDesc_t *SendDescriptor;
    RecvDesc_t *RecvDescriptor;

    int ReturnValue = ULM_SUCCESS;

    // initialize completion status to incomplete
    *completed = 0;

    /*
     * if request is NULL, follow MPI conventions and return success
     * with an "empty" status (tag = any, proc = any, error = success)
     */
    if (*request == NULL) {
        *completed = 1;
        status->tag = ULM_ANY_TAG;
        status->proc.source = ULM_ANY_PROC;
        status->error = ULM_SUCCESS;
        status->state = ULM_STATUS_INVALID;
        status->matchedSize = 0;
        status->persistent = 0;
        return ULM_SUCCESS;
    }

    // get pointer to ULM request object
    tmpRequest = (RequestDesc_t *) (*request);

    // sanity checks
    assert(tmpRequest->status != ULM_STATUS_INVALID);

    // check to see if request has been started
    if (tmpRequest->status == ULM_STATUS_INITED) {
        status->state = ULM_STATUS_INITED;
        return ULM_ERR_BAD_PARAM;
    }

    // move data along
    ReturnValue = ulm_make_progress();
    if ((ReturnValue == ULM_ERR_OUT_OF_RESOURCE) ||
        (ReturnValue == ULM_ERR_FATAL) || (ReturnValue == ULM_ERROR)) {
        return ReturnValue;
    }

    /*
     * for inactive/complete requests follow MPI conventions:
     * - free request
     * - set request pointer to null (done by ulm_request_free)
     * - return success with empty status
     */
    if (tmpRequest->status == ULM_STATUS_COMPLETE ||
        tmpRequest->status == ULM_STATUS_INACTIVE) {
        *completed = 1;
        status->tag = ULM_ANY_TAG;
        status->proc.source = ULM_ANY_PROC;
        status->error = ULM_SUCCESS;
        status->state = tmpRequest->status;
        status->matchedSize = 0;
        status->persistent = tmpRequest->persistent;
        if (!(tmpRequest->persistent)) {
            ulm_request_free(request);
        } else {
            tmpRequest->persistFreeCalled = true;
            tmpRequest->status = ULM_STATUS_INACTIVE;
        }
        return ULM_SUCCESS;
    }
    // Check for message type and handle appropriately
    status->matchedSize = -1;
    switch (tmpRequest->requestType) {

    case REQUEST_TYPE_RECV:
	    RecvDescriptor = (RecvDesc_t *) (*request);
        if (RecvDescriptor->messageDone == REQUEST_INCOMPLETE ) {
            return ULM_SUCCESS;
        }
        // fill in status object
        status->tag = RecvDescriptor->reslts_m.UserTag_m;
        status->proc.source = RecvDescriptor->reslts_m.proc.source_m;
        status->matchedSize = RecvDescriptor->DataReceived;
        status->persistent = RecvDescriptor->persistent;
        status->state = ULM_STATUS_COMPLETE;
        *completed = 1;
        // test for completion
        if (RecvDescriptor->posted_m.length_m != RecvDescriptor->reslts_m.length_m) {
            if (RecvDescriptor->posted_m.length_m >
                RecvDescriptor->reslts_m.length_m) {
                status->error = ULM_ERR_RECV_LESS_THAN_POSTED;
                ReturnValue = ULM_ERR_RECV_LESS_THAN_POSTED;
            } else {
                // truncation error
                status->error = ULM_ERR_RECV_MORE_THAN_POSTED;
                ReturnValue = ULM_ERR_RECV_MORE_THAN_POSTED;
            }
        } else {
            status->error = ULM_SUCCESS;
        }

        break;

    case REQUEST_TYPE_SEND:
	SendDescriptor = (BaseSendDesc_t *) (*request);
        if (SendDescriptor->messageDone == REQUEST_INCOMPLETE ) {
            return ULM_SUCCESS;
        }
        // fill in status object
        status->tag = SendDescriptor->posted_m.UserTag_m;
        status->proc.destination = SendDescriptor->posted_m.proc.destination_m;
        status->state = ULM_STATUS_COMPLETE;
        status->error = ULM_SUCCESS;
        status->matchedSize = SendDescriptor->posted_m.length_m;
	status->persistent = SendDescriptor->persistent;
	*completed = 1;

        break;

    default:
        ulm_err(("Error: ulm_test: Unrecognized message type %d\n",
                 tmpRequest->requestType));
        ReturnValue = ULM_ERR_REQUEST;
    }

    // set request object status to complete, if complete...
    if (*completed) {
        // free request if done, and request is not persistent
        if (!(tmpRequest->persistent)) {
            tmpRequest->status = ULM_STATUS_COMPLETE;
            ulm_request_free(request);
        } else {
            tmpRequest->persistFreeCalled = true;
            tmpRequest->status = ULM_STATUS_INACTIVE;
        }
    }

    return ReturnValue;
}
