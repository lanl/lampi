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

#include <string.h>
#include <strings.h>

#include "queue/globals.h"
#include "util/DblLinkList.h"
#include "util/MemFunctions.h"
#include "util/Utility.h"
#include "util/inline_copy_functions.h"
#include "internal/buffer.h"
#include "internal/constants.h" // for CACHE_ALIGNMENT
#include "internal/log.h"
#include "internal/state.h"
#include "os/atomic.h"
#include "path/common/BaseDesc.h"
#include "path/common/path.h"
#include "path/common/InitSendDescriptors.h"
#include "ulm/ulm.h"

#if ENABLE_SHARED_MEMORY
# include "path/sharedmem/SMPSharedMemGlobals.h"
#endif                          // SHARED_MEMORY

#ifdef DEBUG_DESCRIPTORS
#include "util/dclock.h"

extern double times [8];
extern int sendCount;
extern bool startCount;
extern double tt0;
#endif


// copied frag data to processor address space using a non-contiguous
// datatype as a format
int non_contiguous_copy(BaseRecvFragDesc_t *frag,
                        ULMType_t *datatype,
                        void *dest,
                        ssize_t *length, unsigned int *checkSum)
{
    size_t frag_seq_off = frag->seqOffset_m;
    size_t message_len = frag->msgLength_m;
    size_t frag_len = frag->length_m;
    void *src = frag->addr_m;
    int init_cnt = frag_seq_off / datatype->packed_size;
    int tot_cnt = message_len / datatype->packed_size;
    int tmap_init, ti, dtype_cnt;
    size_t len_to_copy;
    unsigned int lp_int = 0;
    unsigned int lp_len = 0;
    ULMTypeMapElt_t *tmap = datatype->type_map;
    int i;
    void *src_addr;
    void *dest_addr;
    size_t len_copied = 0;

    // find starting typemap index
    tmap_init = datatype->num_pairs - 1;
    for (i = 0; i < datatype->num_pairs; i++) {
        if (init_cnt * datatype->packed_size + tmap[i].seq_offset ==
            frag_seq_off) {
            tmap_init = i;
            break;
        } else if (init_cnt * datatype->packed_size + tmap[i].seq_offset >
                   frag_seq_off) {
            tmap_init = i - 1;
            break;
        }
    }

    // handle initial typemap pair
    src_addr = (void *) ((char *) src);
    dest_addr = (void *) ((char *) dest
                          + init_cnt * datatype->extent
                          + tmap[tmap_init].offset
                          - init_cnt * datatype->packed_size
                          - tmap[tmap_init].seq_offset + frag_seq_off);

    if (frag_len == 0) {
        len_to_copy = 0;
    } else {
        len_to_copy = tmap[tmap_init].size
            + init_cnt * datatype->packed_size
            + tmap[tmap_init].seq_offset - frag_seq_off;
        len_to_copy = (len_to_copy > frag_len) ? frag_len : len_to_copy;
    }

    if (checkSum)
        *checkSum = 0;
    len_copied =
        frag->nonContigCopyFunction(dest_addr, src_addr, len_to_copy,
                                    len_to_copy, checkSum, &lp_int,
                                    &lp_len, true,
                                    (frag_len - len_copied - len_to_copy ==
                                     0) ? true : false);

    if (frag_len - len_copied) {
        tmap_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tmap_init; ti < datatype->num_pairs; ti++) {
                src_addr = (void *) ((char *) src + len_copied);
                dest_addr = (void *) ((char *) dest
                                      + dtype_cnt * datatype->extent
                                      + tmap[ti].offset);

                len_to_copy = (frag_len - len_copied >= tmap[ti].size) ?
                    tmap[ti].size : frag_len - len_copied;
                if (len_to_copy == 0) {
                    *length = len_copied;
                    return ULM_SUCCESS;
                }

                bool lastCall = (frag_len - len_copied - len_to_copy == 0)
                    || ((ti == (datatype->num_pairs - 1))
                        && (dtype_cnt == (tot_cnt - 1)));
                len_copied +=
                    frag->nonContigCopyFunction(dest_addr, src_addr,
                                                len_to_copy, len_to_copy,
                                                checkSum, &lp_int, &lp_len,
                                                false, lastCall);
            }
            tmap_init = 0;
        }
    }
    *length = len_copied;
    return ULM_SUCCESS;
}

/*
 * This routine is called to process fragment sequence numbers.  It is
 *   called from AckData.  There are two circumstances under which
 *   this routine is called:
 *      isDuplicate_m is false - after the fragment has been processed.
 *        In this case, receivedDataSeqs has been updated to include
 *        the sequence number range associated with this packet.  If
 *        the data received is correct, deliveredDataSeqs is also updated
 *        to include the same range of sequence numbers.  If the data
 *        is corrupt, the sequence range is erased from the receivedDataSeqs
 *        list.
 *      isDuplicate_m is true - this is a duplicate fragment.  Another
 *        copy of the fragment is either on a list pending future processing,
 *        or is currently being processed by another thread.  It is also
 *        possible that an ack has been dropped, and the fragment is being
 *        retransmitted.
 */
int BaseRecvFragDesc_t::processRecvDataSeqs(BaseAck_t *ackPtr, 
		int glSourceProcess, ReliabilityInfo *reliabilityData)
{
	int returnValue=ULM_SUCCESS;

#if ENABLE_RELIABILITY

	if( !isDuplicate_m ) {
		/* 
		 * this is called after a fragment's data has been processed 
		 */
		if ((msgType_m == MSGTYPE_PT2PT) || 
				(msgType_m == MSGTYPE_PT2PT_SYNC)) {
			/* fill in delivered data status */
			ackPtr->ackStatus = (DataOK) ? ACKSTATUS_DATAGOOD : 
				ACKSTATUS_DATACORRUPT;

			/* grab lock for sequence tracking lists */
			if (usethreads())
				reliabilityData->dataSeqsLock[glSourceProcess].
					lock();

			/* update sequence tracking lists */
			bool recorded;
			if( ackPtr->ackStatus == ACKSTATUS_DATAGOOD ) {
				/* data is ok - update deliveredDataSeqs list */
				reliabilityData->
					deliveredDataSeqs[glSourceProcess].
					recordIfNotRecorded (seq_m, &recorded);
				if (!recorded) {
					reliabilityData->dataSeqsLock
						[glSourceProcess].unlock();
					ulm_exit((-1, "BaseRecvFragDesc_t::processRecvDataSeqs(pt2pt) unable "
								"to record deliv'd sequence number\n"));
				}
			} else {
				/* data is corrupt - erase sequence data from 
				 * receivedDataSeqs */
				if (!(reliabilityData->receivedDataSeqs
							[glSourceProcess].erase
							(seq_m))) {
					reliabilityData->dataSeqsLock
						[glSourceProcess].unlock();
					ulm_exit((-1, "seRecvFragDesc_t::processRecvDataSeqs(pt2pt) unable "
								"to erase rcv'd sequence number\n"));
				}
			}

			// unlock sequence tracking lists
			if (usethreads())
				reliabilityData->dataSeqsLock[glSourceProcess].unlock();
		} else {
			// unknown communication type
			ulm_exit((-1, "BaseRecvFragDesc_t::processRecvDataSeqs unknown communication "
						"type %d\n", msgType_m));
		}
	} else {
		/* 
		 * This is being called as the result of a duplicate fragment,
		 *   being detected 
		 */

		if( isDuplicate_m == DUPLICATE_DELIVERD ) {
			ackPtr->ackStatus = ACKSTATUS_DATAGOOD;
		} else if (isDuplicate_m == DUPLICATE_RECEIVED ) {
			ackPtr->thisFragSeq = 0;
			ackPtr->ackStatus = ACKSTATUS_AGGINFO_ONLY;
		}


	}

	/* set non-specific ack information */
	ackPtr->receivedFragSeq = reliabilityData->
		receivedDataSeqs[glSourceProcess].largestInOrder();
	ackPtr->deliveredFragSeq = reliabilityData->
		deliveredDataSeqs[glSourceProcess].largestInOrder();

#else
	ackPtr->receivedFragSeq = 0;
	ackPtr->deliveredFragSeq = 0;
	ackPtr->ackStatus = (DataOK) ? ACKSTATUS_DATAGOOD : 
		ACKSTATUS_DATACORRUPT;
#endif

	/* return */
	return returnValue;

}

void SendDesc_t::shallowCopyTo(RequestDesc_t *request)
{
    SendDesc_t	*sendDesc = (SendDesc_t *)request;

    RequestDesc_t::shallowCopyTo(request);
    sendDesc->bsendBufferSize = bsendBufferSize;
    sendDesc->sendType = sendType;
    sendDesc->addr_m = addr_m;
    sendDesc->posted_m = posted_m;

    ulm_type_retain(sendDesc->bsendDatatype);
}


// return number of uncorrupted bytes copied, or -1 if the frag's data
// is corrupt
ssize_t RecvDesc_t::CopyToApp(void *FrgDesc, bool * recvDone)
{
    bool dataNotCorrupted = false;
    ULMType_t *datatype;
    unsigned int checkSum;
    ssize_t bytesIntoBitBucket;

    // frag pointer
    BaseRecvFragDesc_t *FragDesc = (BaseRecvFragDesc_t *) FrgDesc;

    // compute frag offset
    unsigned long Offset = FragDesc->dataOffset();

    // destination buffer
    void *Destination = (void *) ((char *) addr_m + Offset);

    // length to copy
    ssize_t lengthToCopy = FragDesc->length_m;

    // make sure we don't overflow app buffer
    ssize_t AppBufferLen = posted_m.length_m - Offset;

    datatype = this->datatype;

    if (AppBufferLen <= 0) {
        FragDesc->MarkDataOK(true);
        // mark recv as complete
        lengthToCopy=0;
        bytesIntoBitBucket=FragDesc->length_m;
    } else {
        if (AppBufferLen < lengthToCopy) 
            lengthToCopy=AppBufferLen;
        bytesIntoBitBucket=FragDesc->length_m -lengthToCopy;
        if (datatype == NULL || datatype->layout == CONTIGUOUS) {
            // copy the data
            checkSum =
                FragDesc->CopyFunction(FragDesc->addr_m, 
                                       Destination, lengthToCopy);
        } else {                    
            // Non-contiguous case
            non_contiguous_copy(FragDesc, datatype, addr_m, 
                                &lengthToCopy, &checkSum);
        }

        // check to see if data arrived ok
        dataNotCorrupted = FragDesc->CheckData(checkSum, lengthToCopy);

        // return number of bytes copied
        if (!dataNotCorrupted) {
            return (-1);
        }
    }
    DeliveredToApp(lengthToCopy, bytesIntoBitBucket, recvDone);
    return lengthToCopy;
}


void RecvDesc_t::DeliveredToApp(unsigned long bytesCopied, unsigned long bytesDiscarded, bool* recvDone)
{
    // update byte count
    DataReceived += bytesCopied;
    DataInBitBucket += bytesDiscarded;

    // check to see if receive is complete, and if so mark the request object
    //   as such
    if ((DataReceived + DataInBitBucket) >= reslts_m.length_m) {

        // mark recv as complete
        assert(messageDone != REQUEST_COMPLETE);
        messageDone = REQUEST_COMPLETE;
        if (recvDone)
            *recvDone = true;
        wmb();

        // remove recv desc from list
        Communicator *Comm = communicators[ctx_m];
        if (WhichQueue == MATCHEDIRECV) {
            Comm->privateQueues.MatchedRecv[reslts_m.peer_m]
                ->RemoveLink(this);
        }

        // if ulm_request_free has already been called, then 
        // we free the recv/request obj. here
        if (freeCalled)
            requestFree();
    }
}


// return number of uncorrupted bytes copied, or -1 if the frag's
//   data is corrupt - recv descriptor is locked for atomic update of
//   counters
ssize_t RecvDesc_t::CopyToAppLock(void *FrgDesc, bool * recvDone)
{
    bool dataNotCorrupted = false;
    ULMType_t *datatype;
    unsigned int checkSum;
    ssize_t bytesIntoBitBucket;

    // frag pointer
    BaseRecvFragDesc_t *FragDesc = (BaseRecvFragDesc_t *) FrgDesc;

    // compute frag offset
    unsigned long Offset = FragDesc->dataOffset();

    // destination buffer
    void *Destination = (void *) ((char *) addr_m + Offset);

    // length to copy
    ssize_t lengthToCopy = FragDesc->length_m;

    // make sure we don't overflow app buffer
    ssize_t AppBufferLen = posted_m.length_m - Offset;

    //Contiguous Data
    datatype = this->datatype;

    if (AppBufferLen <= 0) {
        FragDesc->MarkDataOK(true);
        // mark recv as complete
        lengthToCopy=0;
        bytesIntoBitBucket=FragDesc->length_m;
    } else {
        if (AppBufferLen < lengthToCopy) 
            lengthToCopy=AppBufferLen;
        bytesIntoBitBucket=FragDesc->length_m -lengthToCopy;
        if (datatype == NULL || datatype->layout == CONTIGUOUS) {
            // copy the data
            checkSum =
                FragDesc->CopyFunction(FragDesc->addr_m, 
                                       Destination, lengthToCopy);
        } else {                    
            // Non-contiguous case
            non_contiguous_copy(FragDesc, datatype, addr_m, 
                                &lengthToCopy, &checkSum);
        }

        // check to see if data arrived ok
        dataNotCorrupted = FragDesc->CheckData(checkSum, lengthToCopy);
        // return number of bytes copied
        if (!dataNotCorrupted) {
            return (-1);
        }
    }
    DeliveredToAppLock(lengthToCopy, bytesIntoBitBucket, recvDone);
    return lengthToCopy;
}


void RecvDesc_t::DeliveredToAppLock(unsigned long bytesCopied, unsigned long bytesDiscarded, bool* recvDone)
{
    // thread lock must be held for the entire time to prevent multiple
    // threads from signaling receive completion for this RecvDesc_t...
    if (usethreads()) {
        Lock.lock();
    } 

    // update byte count
    DataReceived += bytesCopied;
    DataInBitBucket += bytesDiscarded;

    // check to see if receive is complete, and if so mark the request
    // object as such
    if ((DataReceived + DataInBitBucket) >= reslts_m.length_m) {
    // mark recv as complete - the request descriptor will be marked
	// later on.  This is to avoid a race condition with another thread
	// waiting to complete a recv, completing, and try to free the
	// communicator before the current thread is done referencing
	// this communicator
        if (recvDone)
            *recvDone = true;
        wmb();

        // remove receive descriptor from list
        Communicator *Comm = communicators[ctx_m];
        if (WhichQueue == MATCHEDIRECV) {
            Comm->privateQueues.MatchedRecv[reslts_m.peer_m]->RemoveLink(this);
        }

        // if ulm_request_free() has already been called, then we
        // free the recv/request object here...
        if (freeCalled)
            requestFree();
    }

    if (usethreads()) {
        Lock.unlock();
    }
}


#if ENABLE_SHARED_MEMORY

int RecvDesc_t::SMPCopyToApp(unsigned long sequentialOffset,
                                   unsigned long fragLen, void *fragAddr,
                                   unsigned long sendMessageLength,
                                   int *recvDone)
{
    int returnValue = ULM_SUCCESS;
    ssize_t length;
    ssize_t AppBufferLen;
    void *Destination;
    ULMType_t *datatype;

    *recvDone = 0;

    datatype = this->datatype;

    if (datatype == NULL || datatype->layout == CONTIGUOUS) {
        // compute frag offset
        length = fragLen;
        // make sure we don't overflow buffer
        AppBufferLen = posted_m.length_m - sequentialOffset;
        // if AppBufferLen is negative or zero, then there is nothing to
        // copy, so get out of here as soon as possible...
        if (AppBufferLen <= 0) {

            DataInBitBucket += fragLen;
            return ULM_SUCCESS;
        } else if (AppBufferLen < length) {
            length = AppBufferLen;
        }

        Destination = (void *) ((char *) addr_m + sequentialOffset);   // Destination buffer
        MEMCOPY_FUNC(fragAddr, Destination, length);
    } else {
        contiguous_to_noncontiguous_copy(datatype, addr_m, &length,
                                         sequentialOffset, fragLen,
                                         fragAddr);
    }

    // update recv counters
    DataReceived += length;
    DataInBitBucket += (fragLen - length);

    return returnValue;
}

#endif                          // SHARED_MEMORY

void RecvDesc_t::requestFree(void) 
{
    Communicator *commPtr = communicators[ctx_m];

    // decrement request reference count
    if (persistent) {
        commPtr->refCounLock.lock();
        (commPtr->requestRefCount)--;
        commPtr->refCounLock.unlock();
    }

    // reset request parameters
    WhichQueue = REQUESTFREELIST;
    status = ULM_STATUS_INVALID;
    freeCalled = false;
    
    // return request to free list
    if (usethreads()) {
        IrecvDescPool.returnElement(this);
    } else {
        IrecvDescPool.returnElementNoLock(this);
    }

    return;
}


#if ENABLE_RELIABILITY


bool BaseRecvFragDesc_t::checkForDuplicateAndNonSpecificAck(BaseSendFragDesc_t *sfd)
{
    sender_ackinfo_control_t *sptr;
    sender_ackinfo_t *tptr;
    
    // update sender ACK info -- largest in-order delivered and received frag sequence
    // numbers received by our peers (or peer box, in the case of collective communication)
    // from ourself or another local process
    if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        sptr = &(reliabilityInfo->sender_ackinfo[local_myproc()]);
        tptr = &(sptr->process_array[ackSourceProc()]);
    } else {
        ulm_err(("gmRecvFragDesc::check...Ack received ack of unknown "
                 "message type %d\n", msgType_m));
        return true;
    }
    
    sptr->Lock.lock();
    if ( ackDeliveredFragSeq() > tptr->delivered_largest_inorder_seq) {
        tptr->delivered_largest_inorder_seq = ackDeliveredFragSeq();
    }
    // received is not guaranteed to be a strictly increasing series...
    tptr->received_largest_inorder_seq = ackReceivedFragSeq();
    sptr->Lock.unlock();
    
    // check to see if this ACK is for a specific frag or not
    // if not, then we don't need to do anything else...
    if (ackStatus() == ACKSTATUS_AGGINFO_ONLY) {
        return true;
    } else if ( sfd->fragSequence() != ackFragSequence() ) {
        // this ACK is a duplicate...or just screwed up...just ignore it...
        return true;
    }
    
    return false;
}

#endif      /* ENABLE_RELIABILITY */

void BaseRecvFragDesc_t::handlePt2PtMessageAck(double timeNow, SendDesc_t *bsd,
                                   BaseSendFragDesc_t *sfd)
{
    short whichQueue = sfd->WhichQueue;
    
    if (ackStatus() == ACKSTATUS_DATAGOOD) {
        (bsd->NumAcked)++;
        if ( sfd->sendDidComplete() )
            sfd->freeResources(timeNow, bsd);
    } else {
        /*
         * only process negative acknowledgements if we are
         * the process that sent the original message; otherwise,
         * just rely on sender side retransmission
         */
        // move message to incomplete queue
        if ( (bsd->FragsToAck.size() + (unsigned) bsd->NumSent) >= bsd->numfrags) {
            // sanity check, is frag really in UnackedPostedSends queue
            if (bsd->WhichQueue != UNACKEDISENDQUEUE) {
                ulm_exit((-1, "Error: :: Send descriptor not "
                          "in UnackedPostedSends"
                          " list, where it was expected.\n"));
            }
            bsd->WhichQueue = INCOMPLETEISENDQUEUE;
            UnackedPostedSends.RemoveLink(bsd);
            IncompletePostedSends.Append(bsd);
        }

        // reset WhichQueue flag
        sfd->WhichQueue = sfd->parentSendDesc_m->path_m->fragSendQueue();
        // move Frag from FragsToAck list to FragsToSend list
        if ( whichQueue == sfd->parentSendDesc_m->path_m->toAckQueue() ) {
            bsd->FragsToAck.RemoveLink((Links_t *)sfd);
            bsd->FragsToSend.Append((Links_t *)sfd);
        }
        
        // reset send desc. NumSent as though this frag has not been sent
        sfd->setSendDidComplete(false);
        (bsd->NumSent)--;
    } // end NACK/ACK processing
}

