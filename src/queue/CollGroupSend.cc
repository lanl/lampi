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
#include "path/mcast/utsendInit.h"
#include "path/mcast/utsendDesc.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "ulm/ulm.h"

int Communicator::utsend_init(int tag, void *data, int dataLength,
                              ULMType_t *dtype,
			      ULMMcastInfo_t *mcastInfo, int mcastInfoLen,
                              RequestDesc_t*& request)
{
    int listIndex = 0;
    int errorCode = 0;

    if( usethreads() )
	request = ulmRequestDescPool.getElement(listIndex, errorCode);
    else
	request = ulmRequestDescPool.getElementNoLock(listIndex, errorCode);
    if( errorCode != ULM_SUCCESS ){
	return errorCode;
    }

    // sanity check
    request->WhichQueue = REQUESTINUSE;

    // set message type in ReturnHandle (defined in BaseDesc.h)
    request->requestType = REQUEST_TYPE_UTSEND;

    // set datatype pointer
    request->datatype = dtype;

    // set message done
    request->messageDone = false;

    // set buf type, posted size
    if (dtype == NULL) {
	request->posted_m.length_m = dataLength;
    }
    else {
	request->posted_m.length_m = dtype->packed_size * dataLength;
    }

    // set data pointer
    if ((dtype != NULL) && (dtype->layout == CONTIGUOUS) && (dtype->num_pairs != 0)) {
        request->pointerToData = (void *)((char *)data + dtype->type_map[0].offset);
    }
    else {
        request->pointerToData = data;
    }

    // set communication type
    request->messageType = MSGTYPE_COLL;

    // set group id
    request->ctx_m = contextID;

    // set tag
    request->posted_m.UserTag_m = tag;

    // set request to persistent
    request->persistent = false;

    // set request state to inactive
    request->status = ULM_STATUS_INITED;

    // set request send type to standard
    request->sendType = ULM_SEND_STANDARD;

    return ULM_SUCCESS;
}

int Communicator::utsend_start(RequestDesc_t *request,
                               ULMMcastInfo_t *mcastInfo,
			       int mcastInfoLen)
{
    UtsendDesc_t * SendDescriptor;
    int errorCode;
    int index = 0;
    // make sure that the request object is in the inactive state
    if ((request->status == ULM_STATUS_INVALID))
    {
	ulm_err(("Error: Bad request object in Communicator::utsend_start\n"));
	return ULM_ERR_REQUEST;
    }

    SendDescriptor = UtsendDescriptors.getElement(index, errorCode);
    if (errorCode != ULM_SUCCESS)
	return errorCode;

    request->requestDesc = (void *) SendDescriptor;

    // lock descriptor to prevent any other thread from touching it
    // once it goes onto IncompleteUtsendDescriptors in desc->Init()
    SendDescriptor->Lock.lock();
    SendDescriptor->Init(request, mcastInfo, mcastInfoLen);
    errorCode = SendDescriptor->send(true);
    if (errorCode != ULM_SUCCESS) {
	IncompleteUtsendDescriptors[local_myproc()]->RemoveLink(SendDescriptor);
	SendDescriptor->Lock.unlock();
	SendDescriptor->freeDescriptor();
	return errorCode;
    }
    SendDescriptor->Lock.unlock();

    return ULM_SUCCESS;
}
