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

#include <new>
#include <strings.h>

#include "queue/globals.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "path/common/BaseDesc.h"
#include "path/mcast/path.h"
#include "path/mcast/localcollFrag.h"
#include "path/mcast/state.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "ulm/ulm.h"

bool localCollPath::canReach(int globalDestProcessID)
{
    int destinationHostID = global_proc_to_host(globalDestProcessID);
    if (destinationHostID == myhost())
	return true;
    else
	return false;
}

bool localCollPath::bind(BaseSendDesc_t *message, int *globalDestProcessIDArray, int destArrayCount, int *errorCode)
{
    if (isActive() && message->multicastMessage) {
	*errorCode = ULM_SUCCESS;
	message->path_m = this;
	return true;
    }
    else {
	*errorCode = ULM_ERR_BAD_PATH;
	return false;
    }
}

void localCollPath::unbind(BaseSendDesc_t *message, int *globalDestProcessIDArray, int destArrayCount)
{
    ulm_err(("localCollPath::unbind - called, not implemented yet...\n"));
}

bool localCollPath::init(BaseSendDesc_t *message)
{
    unsigned int fragCount;
    fragCount = (message->PostedLength + SizeLargestSMPSharedMemBucket - 1) / SizeLargestSMPSharedMemBucket;
    /* 0 byte message */
    if (fragCount == 0)
	fragCount = 1;
    message->numfrags = fragCount;
    return true;
}

bool localCollPath::send(BaseSendDesc_t *message, bool *incomplete, int *errorCode)
{
    Communicator* commPtr = communicators[message->ctx_m];
    SMPSharedMem_logical_dev_t *dev=&SMPSharedMemDevs[getMemPoolIndex()];
    int destRank = global_to_local_proc(commPtr->localGroup->mapGroupProcIDToGlobalProcID[message->dstProcID_m]);
    void* v;

    *errorCode = ULM_SUCCESS;
    *incomplete = true;

    // allocate local frags as much as possible and as needed
    if (message->NumFragDescAllocated < message->numfrags) {
	int NumDescToAllocate = message->numfrags - message->NumFragDescAllocated;
	for (int ndesc = 0; ndesc < NumDescToAllocate; ndesc++) {
	    // not CTS if more than maxOutstandingLCFDFrags frags have not been acked...
	    if ((maxOutstandingLCFDFrags != -1) && (message->NumFragDescAllocated - message->NumAcked
						    >= (unsigned int)maxOutstandingLCFDFrags)) {
		message->clearToSend_m = false;
	    }
	    // allocate frag only if it is the first or CTS is set...
	    if ((message->NumFragDescAllocated > 0) && (!message->clearToSend_m)) {
		break;
	    }

	    // request descriptor
	    if (allocFrag(message, errorCode)) {
		continue;
	    }
	    else if ((*errorCode == ULM_ERR_OUT_OF_RESOURCE) || (*errorCode == ULM_ERR_FATAL)) {
		return false;
	    }
	    else {
		// try again later...
		break;
	    }
	}
    }

    //   Loop through the frags
    for( LocalCollFragDesc*
	     RecvDesc=(LocalCollFragDesc *)message->FragsToSend.begin() ;
	 RecvDesc!=(LocalCollFragDesc *)message->FragsToSend.end();
	 RecvDesc=(LocalCollFragDesc *)RecvDesc->next )
    {
	// we will succeed on the first try if we get the source shared
	// memory to copy the user data into
	if (RecvDesc->length_m != 0)
	{
	    v=dev->MemoryBuckets.ULMMalloc(RecvDesc->length_m);
	    if (v != (void*)-1L)
	    {
		RecvDesc->addr_m = v;
		RecvDesc->flags |= IO_SOURCEALLOCATED;
		CopyIn(message, RecvDesc);

	    }
	} else {
	    RecvDesc->addr_m = 0;
	    RecvDesc->flags |= IO_SOURCEALLOCATED;
	}

	if ((RecvDesc->flags & IO_SOURCEALLOCATED) == IO_SOURCEALLOCATED)
	{
	    // set WhichQueue indicator
	    RecvDesc->WhichQueue=SORTEDRECVFRAGS;

	    // remove descriptor from list of isend recv descriptors allocated with
	    //   this message
	    LocalCollFragDesc *TmpDesc= (LocalCollFragDesc *)message->FragsToSend.RemoveLinkNoLock(RecvDesc);
	    // add descriptor to list of _ulm_CommunicatorRecvFrags
	    _ulm_CommunicatorRecvFrags[destRank]->AppendAsWriter(RecvDesc);
	    RecvDesc=TmpDesc;
	    (message->NumSent)++;

	}

    }

    // we can declare victory, since this path is "always" reliable
    if (!message->sendDone && (message->NumSent == message->numfrags)) {
	message->sendDone = 1;
	(message->multicastMessage->messageDoneCount)++;
	if (message->multicastMessage->messageDoneCount == message->multicastMessage->numDescsToAllocate) {
	    // the request is in process private memory, but we are in the right context...
	    message->multicastMessage->request->messageDone = true;
	    message->multicastMessage->sendDone = true;
	}
    }

    if (message->NumSent == message->numfrags)
	*incomplete = false;

    return true;
}

void localCollPath::CopyIn(BaseSendDesc_t *message, LocalCollFragDesc *recvFrag)
{
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
	src_addr = ((unsigned char *) message->AppAddr)
	    + recvFrag->fragIndex_m
	    * SizeLargestSMPSharedMemBucket;
	dest_addr = (unsigned char *) recvFrag->addr_m;
	len_to_copy = recvFrag->length_m;

	bcopy(src_addr, dest_addr, len_to_copy);
    }
    else { // data is non-contiguous
	ULMType_t *dtype = message->datatype;
	ULMTypeMapElt_t *tmap = dtype->type_map;
	int tm_init = recvFrag->tmap_index;
	int init_cnt = recvFrag->seqOffset_m
	    / dtype->packed_size;
	int tot_cnt = recvFrag->msgLength_m
	    / dtype->packed_size;
	unsigned char *start_addr = ((unsigned char *) message->AppAddr)
	    + init_cnt * dtype->extent;
	int dtype_cnt, ti;

	// handle first typemap pair
	src_addr = start_addr
	    + tmap[tm_init].offset
	    - init_cnt * dtype->packed_size
	    - tmap[tm_init].seq_offset
	    + recvFrag->seqOffset_m;
	dest_addr = (unsigned char *) recvFrag->addr_m;
	len_to_copy = tmap[tm_init].size
	    + init_cnt * dtype->packed_size
	    + tmap[tm_init].seq_offset
	    - recvFrag->seqOffset_m;
    len_to_copy = (len_to_copy > recvFrag->length_m) ? recvFrag->length_m : len_to_copy;

	bcopy(src_addr, dest_addr, len_to_copy);
	len_copied = len_to_copy;

	tm_init++;
	for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
	    for (ti = tm_init; ti < dtype->num_pairs; ti++) {
		src_addr = start_addr + tmap[ti].offset;
		dest_addr = ((unsigned char *) recvFrag->addr_m)
		    + len_copied;

		len_to_copy = (recvFrag->length_m - len_copied >= tmap[ti].size) ?
		    tmap[ti].size : recvFrag->length_m - len_copied;
		if (len_to_copy == 0) return;

		bcopy(src_addr, dest_addr, len_to_copy);
		len_copied += len_to_copy;
	    }

	    tm_init = 0;
	    start_addr += dtype->extent;
	}
    }
}


bool localCollPath::allocFrag(BaseSendDesc_t *message, int *errorCode)
{
    int memPoolIndex = 0;

    memPoolIndex = getMemPoolIndex();
    *errorCode = ULM_SUCCESS;

    LocalCollFragDesc *FragDesc = (LocalCollFragDesc *)
        LocalCollFragDescriptors.getElement(memPoolIndex, *errorCode);
    if (!FragDesc) {
        return false;
    }
#ifdef _DEBUGQUEUES
    if (FragDesc->WhichQueue != SMPFREELIST) {
        ulm_exit((-1, " FragDesc->WhichQueue != SMPFREELIST "
                  " :: FragDesc->WhichQueue %d\n", FragDesc->WhichQueue));
    }
#endif                          /* _DEBUGQUEUES */
    assert(FragDesc->WhichQueue == SMPFREELIST);

    // fill in pointer to send descriptor
    FragDesc->SendingHeader = message;

    // set message length
    FragDesc->msgLength_m = message->PostedLength;

    // fill in frag size
    size_t LeftToSend =
        message->PostedLength -
        (SizeLargestSMPSharedMemBucket * message->NumFragDescAllocated);
    if (LeftToSend > (size_t) SizeLargestSMPSharedMemBucket) {
        FragDesc->length_m = SizeLargestSMPSharedMemBucket;
    } else {
        FragDesc->length_m = LeftToSend;
    }

    // set index and increment number of frag/frag desc. allocated
    FragDesc->fragIndex_m = (message->NumFragDescAllocated)++;

    // set maxSegSize_m
    FragDesc->maxSegSize_m = SizeLargestSMPSharedMemBucket;

    // set sequential offset
    FragDesc->seqOffset_m = FragDesc->fragIndex_m *
        SizeLargestSMPSharedMemBucket;

    // calculate the typemap index, if the data is non-contiguous
    if (message->datatype != NULL
        && message->datatype->layout == NON_CONTIGUOUS) {
        int dtype_cnt =
            FragDesc->seqOffset_m / message->datatype->packed_size;
        size_t data_copied = dtype_cnt * message->datatype->packed_size;
        ssize_t data_remaining = (ssize_t) (FragDesc->seqOffset_m
                                            - data_copied);
        int ti;

        /* check validity for the following loop */
        assert((dtype_cnt + 1) * message->datatype->packed_size >
               FragDesc->seqOffset_m);

        FragDesc->tmap_index = message->datatype->num_pairs - 1;
        for (ti = 0; ti < message->datatype->num_pairs; ti++) {
            if (message->datatype->type_map[ti].seq_offset ==
                data_remaining) {
                FragDesc->tmap_index = ti;
                break;
            } else if (message->datatype->type_map[ti].seq_offset >
                       data_remaining) {
                FragDesc->tmap_index = ti - 1;
                break;
            }
        }
    } else {
        FragDesc->tmap_index = 0;
    }

    // set source process
    FragDesc->srcProcID_m = myproc();

    // set destination process
    FragDesc->dstProcID_m =
        communicators[message->ctx_m]->localGroup->
        mapGroupProcIDToGlobalProcID[message->dstProcID_m];

    // set user tag and communicator
    FragDesc->tag_m = message->tag_m;
    FragDesc->msgType_m = MSGTYPE_COLL;
    FragDesc->ctx_m = message->ctx_m;

    // set refCnt
    FragDesc->refCnt_m = message->multicastRefCnt;
    // initialize descriptor flag
    FragDesc->flags = 0;
    if (FragDesc->length_m == 0) {
        FragDesc->flags = IO_SOURCEALLOCATED;
        FragDesc->addr_m = 0;
    }
    // set flag indicating which list frag is in
    FragDesc->WhichQueue = SMPFRAGSTOSEND;
    // append frag to FragsToSend
    message->FragsToSend.AppendNoLock(FragDesc);

    return true;
}

