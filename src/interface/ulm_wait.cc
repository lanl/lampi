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

#include "internal/options.h"
#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"
#include "internal/buffer.h"


/*!
 * Wait for posted request to complete
 *
 * \param request       ULM request handle
 * \param status        ULM status struct to fill in
 * \return              ULM return code
 */
extern "C" int ulm_wait(ULMRequestHandle_t *request, ULMStatus_t *status)
{
    int errorCode = ULM_SUCCESS;

    /*
     * if request is NULL, follow MPI conventions and return success
     * with an "empty" status (tag = any, proc = any, error = success)
     */
    if (*request == NULL) {
        status->tag = ULM_ANY_TAG;
        status->proc.source = ULM_ANY_PROC;
        status->error = ULM_SUCCESS;
        status->state = ULM_STATUS_INVALID;
        status->matchedSize = 0;
        status->persistent = 0;
        return ULM_SUCCESS;
    }
    // get pointer to ULM request object, process group
    RequestDesc_t *tmpRequest = (RequestDesc_t *) (*request);

    // sanity checks
    assert(tmpRequest->status != ULM_STATUS_INVALID);

    // check to see if request has be started
    if (tmpRequest->status == ULM_STATUS_INITED) {
        status->state = ULM_STATUS_INITED;
        return ULM_ERR_BAD_PARAM;
    }

    /*
     * for inactive/complete requests follow MPI conventions:
     * - free request
     * - set request pointer to null (done by ulm_request_free)
     * - return success with empty status
     */
    if (tmpRequest->status == ULM_STATUS_COMPLETE ||
        tmpRequest->status == ULM_STATUS_INACTIVE) {
        status->tag = ULM_ANY_TAG;
        status->proc.source = ULM_ANY_PROC;
        status->error = ULM_SUCCESS;
        status->state = tmpRequest->status;
        status->matchedSize = 0;
        status->persistent = tmpRequest->persistent;
        if (!(tmpRequest->persistent)) {
            ulm_request_free(request);
        } else {
            tmpRequest->status = ULM_STATUS_INACTIVE;
        }
        return ULM_SUCCESS;
    }

    /* let the library catch up on needed work */
    errorCode = ulm_make_progress();
    if ((errorCode == ULM_ERR_OUT_OF_RESOURCE)
        || (errorCode == ULM_ERR_FATAL)
        || (errorCode == ULM_ERROR)) {
        return errorCode;
    }

    // Check through all possible cases
    switch (tmpRequest->requestType) {

    case REQUEST_TYPE_RECV:
        while (!(tmpRequest->messageDone)) {
            errorCode = ulm_make_progress();
            if ((errorCode == ULM_ERR_OUT_OF_RESOURCE)
                || (errorCode == ULM_ERR_FATAL)
                || (errorCode == ULM_ERROR)) {
                return errorCode;
            }
        }

        // fill in status object
        status->tag = tmpRequest->reslts_m.UserTag_m;
        status->proc.source = tmpRequest->reslts_m.proc.source_m;
        if (tmpRequest->posted_m.length_m != tmpRequest->reslts_m.length_m) {
            if (tmpRequest->posted_m.length_m >
                tmpRequest->reslts_m.length_m) {
                status->error = ULM_ERR_RECV_LESS_THAN_POSTED;
            } else {
                status->error = ULM_ERR_RECV_MORE_THAN_POSTED;  // truncation error
                errorCode = ULM_ERR_RECV_MORE_THAN_POSTED;
            }
        } else {
            status->error = ULM_SUCCESS;
        }
        status->matchedSize = tmpRequest->reslts_m.lengthProcessed_m;
        status->persistent = tmpRequest->persistent;
        break;

    case REQUEST_TYPE_SEND:
	while (!(tmpRequest->messageDone)) {
	    errorCode = ulm_make_progress();
	    if ((errorCode == ULM_ERR_OUT_OF_RESOURCE)
		|| (errorCode == ULM_ERR_FATAL)
		|| (errorCode == ULM_ERROR)) {
		return errorCode;
	    }
	}

	// fill in status object - same as request
	status->tag = tmpRequest->posted_m.UserTag_m;
	status->proc.destination = tmpRequest->posted_m.proc.destination_m;
	status->error = ULM_SUCCESS;
	status->matchedSize = tmpRequest->posted_m.length_m;
	status->persistent = tmpRequest->persistent;

        break;

    default:
        ulm_err(("Error: ulm_wait: Unrecognized message type: %d\n",
                 tmpRequest->requestType));
        errorCode = ULM_ERR_REQUEST;
    }

    // fill in remainder of status object
    status->state = ULM_STATUS_COMPLETE;

    // return request if it is not persistent
    if (!(tmpRequest->persistent)) {
        tmpRequest->status = ULM_STATUS_COMPLETE;
        ulm_request_free(request);
    } else {
        tmpRequest->status = ULM_STATUS_INACTIVE;
    }

    return errorCode;
}
