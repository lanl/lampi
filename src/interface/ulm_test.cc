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

#include "internal/buffer.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "queue/globals.h"
#include "ulm/ulm.h"

/*!
 * Test posted request for completion without blocking
 *
 * \param request       Request to test for completion
 * \param completed     Flag to set if request is complete
 * \param status        Status struct to be filled in on completion
 * \return              ULM return code
 */
extern "C" int ulm_test(ULMRequest_t *request, int *completed,
                        ULMStatus_t *status)
{
    RequestDesc_t *RequestDesc;
    SendDesc_t *SendDesc;
    RecvDesc_t *RecvDesc;

    int rc = ULM_SUCCESS;

    // initialize completion status to incomplete
    *completed = 0;

    // if request is NULL, follow MPI conventions and return success
    // with an "empty" status (tag = any, proc = any, error = success)
    if (*request == NULL) {
        *completed = 1;
        status->tag_m = ULM_ANY_TAG;
        status->peer_m = ULM_ANY_PROC;
        status->error_m = ULM_SUCCESS;
        status->state_m = ULM_STATUS_INVALID;
        status->length_m = 0;
        status->persistent_m = 0;
        return ULM_SUCCESS;
    }

    // get pointer to ULM request object
    RequestDesc = (RequestDesc_t *) (*request);

    // sanity checks
    assert(RequestDesc->status != ULM_STATUS_INVALID);

    // check to see if request has been started
    if (RequestDesc->status == ULM_STATUS_INITED) {
        status->state_m = ULM_STATUS_INITED;
        return ULM_ERR_BAD_PARAM;
    }

    // move data along
    rc = ulm_make_progress();
    if ((rc == ULM_ERR_OUT_OF_RESOURCE) ||
        (rc == ULM_ERR_FATAL) || (rc == ULM_ERROR)) {
        return rc;
    }

    // for inactive/complete requests follow MPI conventions:
    // - free request
    // - set request pointer to null (done by ulm_request_free)
    // - return success with empty status
    if (RequestDesc->status == ULM_STATUS_COMPLETE ||
        RequestDesc->status == ULM_STATUS_INACTIVE) {
        *completed = 1;
        status->tag_m = ULM_ANY_TAG;
        status->peer_m = ULM_ANY_PROC;
        status->error_m = ULM_SUCCESS;
        status->state_m = RequestDesc->status;
        status->length_m = 0;
        status->persistent_m = RequestDesc->persistent;
        if (!(RequestDesc->persistent)) {
            ulm_request_free(request);
        } else {
            RequestDesc->persistFreeCalled = true;
            RequestDesc->status = ULM_STATUS_INACTIVE;
        }
        return ULM_SUCCESS;
    }

    // Check for message type and handle appropriately
    status->length_m = -1;
    switch (RequestDesc->requestType) {

    case REQUEST_TYPE_RECV:
        RecvDesc = (RecvDesc_t *) (*request);
        if (RecvDesc->messageDone == REQUEST_INCOMPLETE ) {
            return ULM_SUCCESS;
        }
        // fill in status object
        status->tag_m = RecvDesc->reslts_m.tag_m;
        status->peer_m = RecvDesc->reslts_m.peer_m;
        status->length_m = RecvDesc->DataReceived;
        status->persistent_m = RecvDesc->persistent;
        status->state_m = ULM_STATUS_COMPLETE;
        *completed = 1;
        // test for completion
        if (RecvDesc->posted_m.length_m != RecvDesc->reslts_m.length_m) {
            if (RecvDesc->posted_m.length_m >
                RecvDesc->reslts_m.length_m) {
                status->error_m = ULM_ERR_RECV_LESS_THAN_POSTED;
                rc = ULM_ERR_RECV_LESS_THAN_POSTED;
            } else {
                // truncation error
                status->error_m = ULM_ERR_RECV_MORE_THAN_POSTED;
                rc = ULM_ERR_RECV_MORE_THAN_POSTED;
            }
        } else {
            status->error_m = ULM_SUCCESS;
        }

        break;

    case REQUEST_TYPE_SEND:
	SendDesc = (SendDesc_t *) (*request);
        if (SendDesc->messageDone == REQUEST_INCOMPLETE ) {
            return ULM_SUCCESS;
        }
        // fill in status object
        status->tag_m = SendDesc->posted_m.tag_m;
        status->peer_m = SendDesc->posted_m.peer_m;
        status->state_m = ULM_STATUS_COMPLETE;
        status->error_m = ULM_SUCCESS;
        status->length_m = SendDesc->posted_m.length_m;
	status->persistent_m = SendDesc->persistent;
	*completed = 1;

        break;

    default:
        ulm_err(("Error: ulm_test: Unrecognized message type %d\n",
                 RequestDesc->requestType));
        rc = ULM_ERR_REQUEST;
    }

    // set request object status to complete, if complete...
    if (*completed) {
        // free request if done, and request is not persistent
        if (!(RequestDesc->persistent)) {
            RequestDesc->status = ULM_STATUS_COMPLETE;
            ulm_request_free(request);
        } else {
            RequestDesc->persistFreeCalled = true;
            RequestDesc->status = ULM_STATUS_INACTIVE;
        }
    }

    return rc;
}
