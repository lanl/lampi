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
#include <sys/types.h>

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

int Communicator::utrecv_init(int tag, void *data, size_t dataLength,
			      ULMType_t *dtype, RequestDesc_t*& request)
{
    // get request object
    int errorCode;
    int listIndex = 0;

    if( usethreads() )
	request = ulmRequestDescPool.getElement(listIndex, errorCode);
    else
	request = ulmRequestDescPool.getElementNoLock(listIndex, errorCode);

    if( errorCode != ULM_SUCCESS ) return errorCode;

    // sanity check
    assert(request->WhichQueue == REQUESTFREELIST);
    request->WhichQueue = REQUESTINUSE;

    // set done flag to false
    request->messageDone = false;

    // set message type in ReturnHandle
    request->requestType = REQUEST_TYPE_UTRECV;

    // set datatype pointer
    request->datatype = dtype;

    // set posted length, layout
    if (dtype == NULL) {
	request->posted_m.length_m = dataLength ;
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

    return ULM_SUCCESS;
}

int Communicator::utrecv_start(RequestDesc_t *request)
{
    // make sure that the request object is in the inactive state
    if (request->status == ULM_STATUS_INVALID) {
	ulm_err(("Error: irecv_start - ULM request structure is "
		 "in invalid state.\n State :: %d\n", request->status));
	return ULM_ERR_REQUEST;
    }

    // get pointer to send descriptor base class
    RecvDesc_t *recvDesc;

    // actually get the descriptor
    int errorCode;

    if( usethreads() )
	recvDesc = IrecvDescPool.getElement(0, errorCode);
    else
	recvDesc = IrecvDescPool.getElementNoLock(0, errorCode);

    if (errorCode != ULM_SUCCESS)
	return errorCode;

    // fill in descriptor
    recvDesc->requestDesc = request;
    recvDesc->PostedLength = request->posted_m.length_m;
    recvDesc->ReceivedMessageLength = 0;
    recvDesc->DataReceived = 0;
    recvDesc->DataInBitBucket = 0;
    recvDesc->tag_m = request->posted_m.UserTag_m;
    recvDesc->messageType = request->messageType;
    recvDesc->ctx_m = request->ctx_m; // used in CopyToApp()
    recvDesc->AppAddr = request->pointerToData;

    // fill in request
    request->requestDesc = (void *) recvDesc;

    // change status
    request->status = ULM_STATUS_INCOMPLETE;

    // initialize bit vector and make sure it is zero'd out
    bv_init(&(recvDesc->fragsProcessed), 128);
    bv_clearall(&(recvDesc->fragsProcessed));

    // init fragMatched to false
    recvDesc->fragMatched = false;

    // Append receive to privateQueues.PostedUtrecvs list
    privateQueues.PostedUtrecvs.Lock.lock();
    recvDesc->WhichQueue = POSTEDUTRECVS;
    privateQueues.PostedUtrecvs.AppendNoLock(recvDesc);
    privateQueues.PostedUtrecvs.Lock.unlock();

    // process frags
    ulm_make_progress();

    return ULM_SUCCESS;
}

// check all posted receives against frags
// in _ulm_CommunicatorRecvFrags
int Communicator::processCollectiveFrags()
{
    int retVal = ULM_SUCCESS;
    if (privateQueues.PostedUtrecvs.size() == 0) return retVal;

    // loop over all posted receives
    RecvDesc_t *recvDesc = 0;
    BaseRecvFragDesc_t *fragDesc = 0;
    BaseRecvFragDesc_t *tmpFragDesc = 0;
    SharedMemDblLinkList *List;
    int rt = 0;
    Communicator *grp = 0;
    long bytesCopied = 0;
    bool	can_clean  = false ;
    int fetchedRefCnt;

    privateQueues.PostedUtrecvs.Lock.lock();
    for (recvDesc = (RecvDesc_t *) privateQueues.PostedUtrecvs.begin();
	 recvDesc != (RecvDesc_t *) privateQueues.PostedUtrecvs.end();
	 recvDesc = (RecvDesc_t *) recvDesc->next) {

	// grab the root
	grp = communicators[recvDesc->ctx_m];
	rt = global_to_local_proc(grp->localGroup->groupHostData[grp->localGroup->hostIndexInGroup].destProc);
	List = (SharedMemDblLinkList*)_ulm_CommunicatorRecvFrags[rt];

	// grab the relevant list and loop over it
	if (List->size() == 0) continue;

	// determine whether the reader (proc) is a normal reader
	// that is it just reads info from the list
	// or a cleaner which will remove things from the list
	List->Lock.lock();
	if (List->needs_cleaning && List->readers == 0 && List->writers == 0)
	    can_clean = true;
	else {
	    while (List->writers) {
		List->Lock.unlock();
		while (List->writers) ;
		List->Lock.lock();
	    }
	    List->readers++;
	    can_clean = false;
	    List->Lock.unlock();
	}

	for (fragDesc = (BaseRecvFragDesc_t *) List->begin();
	     fragDesc != (BaseRecvFragDesc_t *) List->end();
	     fragDesc = (BaseRecvFragDesc_t *) fragDesc->next)
	{

	    // check for group ID, tag match, frag already processed
	    if (recvDesc->ctx_m == fragDesc->ctx_m &&
		recvDesc->tag_m == fragDesc->tag_m &&
		fragDesc->refCnt_m > 0 &&
		!bv_isset(&(recvDesc->fragsProcessed),
			  fragDesc->fragIndex_m))
	    {

		// fill in received data information
		recvDesc-> ReceivedMessageLength = fragDesc->msgLength_m;
		unsigned long amountToRecv = fragDesc->msgLength_m;
		if (amountToRecv > recvDesc->PostedLength)
		    amountToRecv = recvDesc->PostedLength;
		recvDesc->actualAmountToRecv_m = amountToRecv;
		recvDesc->srcProcID_m = remoteGroup->mapGlobalProcIDToGroupProcID[fragDesc->srcProcID_m];
		recvDesc->fragMatched = true;

		// copy data to user space...CopyToApp handles received byte count, etc.
		RequestDesc_t *requestDesc = recvDesc->requestDesc;
		// save previous descriptor just in case CopyToApp() frees recvDesc
		RecvDesc_t *tmpDesc = (RecvDesc_t *)recvDesc->prev;
		bytesCopied = recvDesc->CopyToApp((void *) fragDesc);

		// CopyToApp() may have freed recvDesc, so we use the
		// requestDesc to check the status of this message...
		if (!requestDesc->messageDone) {
		    if (bytesCopied >= 0) {
			// toggle appropriate bit in fd_set
			bv_set(&(recvDesc->fragsProcessed),
			       fragDesc->fragIndex_m);
		    }
		} else {
		    recvDesc = tmpDesc;
		}

		// update reference count
		// if can_clean flag is not set
		// then acquire the lock first
		if (bytesCopied >= 0) {
		    fetchedRefCnt = fetchNadd((volatile int *)&fragDesc->refCnt_m, -1);
		    if (!can_clean && fetchedRefCnt == 1)
			fetchNadd(&List->needs_cleaning, 1);
		}
		else {
		    fetchedRefCnt = fetchNset((volatile int *)&fragDesc->refCnt_m, 0);
		    if (!can_clean && fetchedRefCnt > 0)
			fetchNadd(&List->needs_cleaning, 1);
		}
	    }

	    if (can_clean == true && fragDesc->refCnt_m <= 0)
            {
                // remove frag from _ulm_CommunicatorRecvFrags
                tmpFragDesc = (BaseRecvFragDesc_t *)List->RemoveLinkNoLock(fragDesc);

                // free frag and generate ack
                if (fragDesc->AckData()) {
                    fragDesc->ReturnDescToPool(rt);
                } else {
                    fragDesc->WhichQueue = FRAGSTOACK;
                    UnprocessedAcks.Append(fragDesc);
                }

                fragDesc = tmpFragDesc;
	    }


	}			// for (fragDesc = (BaseRecvF....)

	// check if we should clean up this list...since
	// nobody may be coming back to clean up
	if (!can_clean && (List->needs_cleaning == List->size())) {
	    List->Lock.lock();
	    List->readers--;
	    if (List->needs_cleaning && (List->needs_cleaning == List->size())
		&& List->writers == 0) {
		while (List->readers) {
		    List->Lock.unlock();
		    while (List->readers) ;
		    List->Lock.lock();
		}
		if (List->needs_cleaning && (List->needs_cleaning == List->size())
		    && List->writers == 0) {
		    for (fragDesc = (BaseRecvFragDesc_t *) List->begin();
			 fragDesc != (BaseRecvFragDesc_t *) List->end();
			 fragDesc = (BaseRecvFragDesc_t *) fragDesc->next) {
			if (fragDesc->refCnt_m <= 0) {
			    // remove frag from _ulm_CommunicatorRecvFrags
			    tmpFragDesc = (BaseRecvFragDesc_t *)List->RemoveLinkNoLock(fragDesc);

			    // free frag and generate ack
			    if (fragDesc->AckData()) {
				fragDesc->ReturnDescToPool(rt);
			    } else {
				fragDesc->WhichQueue = FRAGSTOACK;
				UnprocessedAcks.Append(fragDesc);
			    }
			    fragDesc = tmpFragDesc;
			}
		    }
		    List->needs_cleaning = 0;
		}
	    }
	    List->Lock.unlock();
	    continue;
	}

	// Unregister
	if (can_clean) {
	    List->needs_cleaning = 0;
	    List->Lock.unlock();
	} else {
	    List->Lock.lock();
	    List->readers--;
	    List->Lock.unlock();
	}

    }			// for (recvDesc = (RecvDesc_t *)...

    privateQueues.PostedUtrecvs.Lock.unlock();

    return retVal;
}
