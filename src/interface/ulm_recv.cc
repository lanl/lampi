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

/*!
 * Post non-blocking receive
 *
 * \param buf		Buffer for received buf
 * \param size		Size of buffer in bytes if dtype is NULL,
 *			else number of datatype descriptors
 * \param dtype		Datatype descriptor
 * \param source	ProcID of source process
 * \param tag		Tag for matching
 * \param comm		ID of communicator
 * \param status	ULM status handle
 * \return		ULM return code
 */
extern "C" int ulm_recv(void *buf, size_t size, ULMType_t *dtype,
                        int sourceProc, int tag, int comm,
                        ULMStatus_t *status)
{
    ULMRequest_t requestPtr;
    RecvDesc_t recvDesc;
    RequestDesc_t *request=(RequestDesc_t *)(&recvDesc);
    Communicator *commPtr;
    int errorCode;

    commPtr = communicators[comm];

    // set done flag to false
    recvDesc.messageDone = REQUEST_INCOMPLETE;

    // set ulm_free_request() called flag to false
    recvDesc.freeCalled = false;

    // sanity check
    recvDesc.WhichQueue = REQUESTINUSE;

    // set data type
    recvDesc.datatype = dtype;

    // set receive address
    if ((dtype != NULL) && (dtype->layout == CONTIGUOUS) && (dtype->num_pairs != 0)) {
        recvDesc.addr_m = (void *)((char *)buf + dtype->type_map[0].offset);
    }
    else {
        recvDesc.addr_m = buf;
    }

    // set destination process (need to check for completion)
    recvDesc.posted_m.peer_m = sourceProc;

    // set communicator
    recvDesc.ctx_m = comm;

    // set tag
    recvDesc.posted_m.tag_m = tag;

    // set posted length
    if (dtype == NULL) {
        recvDesc.posted_m.length_m = size;
    } else {
        recvDesc.posted_m.length_m = dtype->packed_size * size;
    }

    // set request state to inactive
    recvDesc.status = ULM_STATUS_INITED;

    // start the recv
    requestPtr=request;
    errorCode = commPtr->irecv_start(&requestPtr);
    if (errorCode != ULM_SUCCESS) {
        return errorCode;
    }

    // wait  - no need to call progress engine here, since irecv_start
    //   just called it
    while ((recvDesc.messageDone == REQUEST_INCOMPLETE)) {
        errorCode = ulm_make_progress();
        if ((errorCode == ULM_ERR_OUT_OF_RESOURCE)
            || (errorCode == ULM_ERR_FATAL)
            || (errorCode == ULM_ERROR)) {
            return errorCode;
        }
    }

    // fill in status object
    status->tag_m = recvDesc.reslts_m.tag_m;
    status->peer_m = recvDesc.reslts_m.peer_m;
    if (recvDesc.posted_m.length_m != recvDesc.reslts_m.length_m) {
        if (recvDesc.posted_m.length_m > recvDesc.reslts_m.length_m) {
            status->error_m = ULM_ERR_RECV_LESS_THAN_POSTED;
        } else {
            // truncation error
            status->error_m = ULM_ERR_RECV_MORE_THAN_POSTED;
        }
    } else {
        status->error_m = ULM_SUCCESS;
    }
    status->length_m = recvDesc.DataReceived;
    status->persistent_m = 0;

    // fill in remainder of status object
    status->state_m = ULM_STATUS_COMPLETE;

    return ULM_SUCCESS;
}
