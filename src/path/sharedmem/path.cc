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

#include <assert.h>

#include "queue/globals.h"
#include "path/sharedmem/path.h"
#include "internal/state.h"
#include "SMPSharedMemGlobals.h"
#include "path/common/InitSendDescriptors.h"

static int maxOutstandingSMPFrags = 30;

inline bool sharedmemPath::canReach(int globalDestProcessID)
{
    // return true only for processes not on our "box"

    int destinationHostID;
    bool returnValue=false;

    destinationHostID = global_proc_to_host(globalDestProcessID);
    if (myhost() == destinationHostID)
	    returnValue=true;

    return returnValue;
}

// initialization function - first frag is posted on the receive side
bool sharedmemPath::init(BaseSendDesc_t *message)
{
    // For all the pages in the send request  - need a minimum of 1 page
    unsigned int FragCount;
    if (message->PostedLength <= SMPFirstFragPayload) {
        FragCount = 1;
    } else {
        FragCount =
            ((message->PostedLength - SMPFirstFragPayload) + SMPSecondFragPayload - 1) /
            SMPSecondFragPayload + 1;
    }

    // Allocate irecv descriptors - these will be put directly into the
    //   queues, for processing by the receive side.
    //   we assume that the recyled descriptors have an empty queue.

    // initialize descriptor
    message->numfrags = FragCount;
    message->NumAcked = 0;
    message->NumSent = 0;
    message->pathInfo.sharedmem.sharedData->matchedRecv = 0;
    message->pathInfo.sharedmem.sharedData->NumAcked = 0;
    message->sendDone = (message->sendType == ULM_SEND_BUFFERED) ? 1 : 0;

    // get communicator pointer
    Communicator *commPtr = (Communicator *) communicators[message->ctx_m];

    // recv process's queue
    int SortedRecvFragsIndex =
        commPtr->remoteGroup->mapGroupProcIDToGlobalProcID[message->dstProcID_m];
    SortedRecvFragsIndex = global_to_local_proc(SortedRecvFragsIndex);

    // we're sending to ourselves and we're the only shared memory process so we need
    // to set the boolean latch to true so that we can pick up the message
    // in ulm_make_progress
    extern bool _ulm_checkSMPInMakeProgress;
    if (!_ulm_checkSMPInMakeProgress
        && (SortedRecvFragsIndex == lampiState.local_rank)) {
        _ulm_checkSMPInMakeProgress = true;
    }
    // process as much as possible of the first frag

    // get first fragement descriptor - when a send is initialized,
    //   freeFrags is guaranteed to be non-empty

    // set index
    message->pathInfo.sharedmem.firstFrag->fragIndex_m = 0;

    // set tmap index
    message->pathInfo.sharedmem.firstFrag->tmapIndex_m = 0;

    // set source process
    int myRank = commPtr->localGroup->ProcID;
    message->pathInfo.sharedmem.firstFrag->srcProcID_m = myRank;

    // set destination process
    message->pathInfo.sharedmem.firstFrag->dstProcID_m = message->dstProcID_m;

    // set user tag and communicator and related fields
    message->pathInfo.sharedmem.firstFrag->tag_m = message->tag_m;
    message->pathInfo.sharedmem.firstFrag->ctx_m = message->ctx_m;

    // set message length
    message->pathInfo.sharedmem.firstFrag->msgLength_m = message->PostedLength;

    //
    // special case - 0 byte message
    //
    if (message->PostedLength == 0) {
        // set frag length
        message->pathInfo.sharedmem.firstFrag->length_m = 0;
        message->NumSent = 1;
        // don't allow frag payload to be read yet, no buffer allocated
        //   and data not copied in
        message->pathInfo.sharedmem.firstFrag->okToReadPayload_m = false;
        // memory barrier before posting...
        mb();

        // update number of descriptors allocated
        message->NumFragDescAllocated = 1;

        // ZERO LENGTH
        // append to destination list

        int sizeOfSendsToPost = SMPSendsToPost[(local_myproc())]->size();
        int senderID = local_myproc();
        int receiverID = SortedRecvFragsIndex;
        if (sizeOfSendsToPost == 0) {
            int slot;
            if (usethreads()) {
                slot =
                    SharedMemIncomingFrags[(senderID)][(receiverID)]->
                    writeToHead(&(message->pathInfo.sharedmem.firstFrag));
                if (slot == CB_ERROR) {
                    SMPSendsToPost[(senderID)]->Append((message->pathInfo.sharedmem.firstFrag));
                }
            } else {
                mb();
                slot =
                    SharedMemIncomingFrags[(senderID)][(receiverID)]->
                    writeToHeadNoLock(&(message->pathInfo.sharedmem.firstFrag));
                if (slot == CB_ERROR) {
                    SMPSendsToPost[(senderID)]->AppendNoLock((message->pathInfo.sharedmem.firstFrag));
                }
                mb();
            }
        } else {
            if (usethreads())
                SMPSendsToPost[(senderID)]->Append((message->pathInfo.sharedmem.firstFrag));
            else {
                mb();
                SMPSendsToPost[(senderID)]->AppendNoLock((message->pathInfo.sharedmem.firstFrag));
                mb();
            }
        }


        // check to see if send is done (buffered already "done";
        // synchronous sends are not done until the first frag is acked)
        if (!(message->sendDone) && message->sendType != ULM_SEND_SYNCHRONOUS) {
            message->requestDesc->messageDone = true;
            message->sendDone = 1;
        }
        // return
        return true;

    }
    // fill in frag size
    size_t LeftToSend = message->PostedLength;
    if (LeftToSend > SMPFirstFragPayload) {
        message->pathInfo.sharedmem.firstFrag->length_m = SMPFirstFragPayload;
    } else {
        message->pathInfo.sharedmem.firstFrag->length_m = LeftToSend;
    }

    // set sequential offset
    message->pathInfo.sharedmem.firstFrag->seqOffset_m = 0;

#ifdef _DEBUGQUEUES
    // set flag indicating which list frag is in
    message->pathInfo.sharedmem.firstFrag->WhichQueue = SMPFRAGSTOSEND;
#endif                          // _DEBUGQUEUES

    // don't allow frag payload to be read yet, no buffer allocated
    //   and data not copied in
    message->pathInfo.sharedmem.firstFrag->okToReadPayload_m = false;

    // initialize descriptor flag
    message->pathInfo.sharedmem.firstFrag->flags_m = 0;
    // memory barrier before posting...
    mb();

    // append to destination list
    int sizeOfSendsToPost = SMPSendsToPost[(local_myproc())]->size();
    int senderID = local_myproc();
    int receiverID = SortedRecvFragsIndex;
    if (sizeOfSendsToPost == 0) {
        int slot;
        if (usethreads()) {
            slot = SharedMemIncomingFrags[(senderID)][(receiverID)]->
                writeToHead(&(message->pathInfo.sharedmem.firstFrag));
            if (slot == CB_ERROR) {
                SMPSendsToPost[(senderID)]->Append((message->pathInfo.sharedmem.firstFrag));
            }
        } else {
            mb();
            slot = SharedMemIncomingFrags[(senderID)][(receiverID)]->
                writeToHeadNoLock(&(message->pathInfo.sharedmem.firstFrag));
            if (slot == CB_ERROR) {
                SMPSendsToPost[(senderID)]->AppendNoLock((message->pathInfo.sharedmem.firstFrag));
            }
            mb();
        }

    } else {
        if (usethreads())
            SMPSendsToPost[(senderID)]->Append((message->pathInfo.sharedmem.firstFrag));
        else {
            mb();
            SMPSendsToPost[(senderID)]->AppendNoLock((message->pathInfo.sharedmem.firstFrag));
            mb();
        }
    }

    // update number of descriptors allocated
    message->NumFragDescAllocated = 1;


    // set flag indicating memory is allocated
    message->pathInfo.sharedmem.firstFrag->flags_m |= IO_SOURCEALLOCATED;
    message->pathInfo.sharedmem.CopyToULMBuffers
	    (message->pathInfo.sharedmem.firstFrag,message);

    // lock the sender to make sure that message->NumSent does not change until
    //   we have set matchedRecv on the receive side
    wmb();
    message->pathInfo.sharedmem.firstFrag->okToReadPayload_m = true;
    /* memory barrier - need to make sure that okToReadPayload is set
     * before message->NumSent
     */
    wmb();
    message->NumSent = 1;

    // check to see if send is done (buffered sends are already
    // "done, and synchronous send are done when the first frag
    // is acknowledged)
    if (!(message->sendDone) &&
        (message->NumSent == message->numfrags) && 
	(message->sendType != ULM_SEND_SYNCHRONOUS)) {
        message->requestDesc->messageDone = true;
        message->sendDone = 1;
    }

    return true;
}

// continue sending, may be called multiple times per send descriptor
bool sharedmemPath::send(BaseSendDesc_t *message, bool *incomplete,
		int *errorCode)
{

	*errorCode=ULM_SUCCESS;
	*incomplete=true;
	int errCode;
    //
    // determine which pool to use
    //
    // group pointer
    Communicator *commPtr = (Communicator *) communicators[message->ctx_m];

    // recv process's queue
    int SortedRecvFragsIndex =
        commPtr->remoteGroup->mapGroupProcIDToGlobalProcID[message->dstProcID_m];
    SortedRecvFragsIndex = global_to_local_proc(SortedRecvFragsIndex);

#ifdef USE_DEST_MEM
    // locality index
    int memPoolIndex = global_to_local_proc(memPoolIndex);
#else
    // locality index
    int memPoolIndex = getMemPoolIndex();
#endif

    //
    // process the rest of the frags
    //

    //
    // allocate frag descriptors, if need be - the assumption is that
    //   these descriptors will be allocated sequentially, and therefore
    //   we can use the order to get offsets with in the message.
    //
    if (message->NumFragDescAllocated < message->numfrags) {
        int NumDescToAllocate = message->numfrags - message->NumFragDescAllocated;
        for (int ndesc = 0; ndesc < NumDescToAllocate; ndesc++) {

            int waitOnAck =
                (message->requestDesc->sendType == ULM_SEND_SYNCHRONOUS);

            // slow down the send
            if ((maxOutstandingSMPFrags != -1) &&
                (message->NumFragDescAllocated - message->NumAcked >=
                 (unsigned) maxOutstandingSMPFrags)) {
                message->pathInfo.sharedmem.sharedData->clearToSend_m = false;
            } else if (!(waitOnAck && message->NumAcked == 0)) {
                message->pathInfo.sharedmem.sharedData->clearToSend_m = true;
            }
            // Request To Send/Clear To Send
            if (!message->pathInfo.sharedmem.sharedData->clearToSend_m) {
                break;
            }
            // request desriptor
            SMPSecondFragDesc_t *FragDesc;

            // first try and get "cached" descriptor
            FragDesc = (SMPSecondFragDesc_t *) 
		    message->pathInfo.sharedmem.sharedData->
		    freeFrags.GetLastElement();
            if (!FragDesc) {

                // get one from the free pool

                FragDesc = (SMPSecondFragDesc_t *)
                    (SMPSecondFragDesc_t *) SMPFragPool.
                    getElement(memPoolIndex, errCode);
                if (errCode == ULM_ERR_OUT_OF_RESOURCE){
			*errorCode=errCode;
			return false;
		}
                if (FragDesc == (SMPSecondFragDesc_t *) 0) {
                    break;
                }

            }
            // sanity check
#ifdef _DEBUGQUEUES
            assert(FragDesc->WhichQueue == SMPFREELIST);
            if (FragDesc->WhichQueue != SMPFREELIST) {
                ulm_exit((-1, " FragDesc->WhichQueue != SMPFREELIST "
                          " :: FragDesc->WhichQueue %d\n",
                          FragDesc->WhichQueue));
            }
#endif                          /* _DEBUGQUEUES */

            // fill in pointer to send descriptor
            FragDesc->SendingHeader_m.SMP = message;

            // set message length
            FragDesc->msgLength_m = message->PostedLength;

            // fill in frag size
            size_t LeftToSend = (message->PostedLength - SMPFirstFragPayload) -
                SMPSecondFragPayload * (message->NumFragDescAllocated - 1);
            if (LeftToSend > (size_t) SMPSecondFragPayload) {
                FragDesc->length_m = SMPSecondFragPayload;
            } else {
                FragDesc->length_m = LeftToSend;
            }

            // set sequential offset - message->NumFragDescAllocated has not yet been
            //  incremented
            FragDesc->seqOffset_m = SMPFirstFragPayload +
                (message->NumFragDescAllocated - 1) * SMPSecondFragPayload;

            // set typemap index if data is non-contiguous
            if (message->datatype != NULL && message->datatype->layout == NON_CONTIGUOUS) {
                int dtype_cnt = FragDesc->seqOffset_m
                    / message->datatype->packed_size;
                size_t data_copied = dtype_cnt * message->datatype->packed_size;
                ssize_t data_remaining =
                    (ssize_t) (FragDesc->seqOffset_m - data_copied);
                int ti;

                /* check validity for the following loop */
                assert((dtype_cnt + 1) * message->datatype->packed_size >
                       FragDesc->seqOffset_m);

                FragDesc->tmapIndex_m = message->datatype->num_pairs - 1;
                for (ti = 0; ti < message->datatype->num_pairs; ti++) {
                    if (message->datatype->type_map[ti].seq_offset ==
                        data_remaining) {
                        FragDesc->tmapIndex_m = ti;
                        break;
                    } else if (message->datatype->type_map[ti].seq_offset >
                               data_remaining) {
                        FragDesc->tmapIndex_m = ti - 1;
                        break;
                    }
                }
            } else {
                FragDesc->tmapIndex_m = 0;
            }

#ifdef _DEBUGQUEUES
            // set flag indicating which list frag is in
            FragDesc->WhichQueue = SMPFRAGSTOSEND;
#endif                          // _DEBUGQUEUES

            // update number of descriptors allocated
            (message->NumFragDescAllocated)++;

            // set flag indicating memory is allocated
            FragDesc->SendingHeader_m.SMP->pathInfo.sharedmem.
		    CopyToULMBuffers(FragDesc,message);
            // lock fragsReadyToSend
	    message->pathInfo.sharedmem.sharedData->fragsReadyToSend.Lock.lock();

            // queue frag
            if (message->pathInfo.sharedmem.sharedData->matchedRecv) {
                // match already made
                FragDesc->matchedRecv_m = message->pathInfo.sharedmem.sharedData->matchedRecv;
                wmb();
                SMPMatchedFrags[SortedRecvFragsIndex]->Append(FragDesc);
            } else {
                wmb();
                message->pathInfo.sharedmem.sharedData->fragsReadyToSend.AppendNoLock(FragDesc);
            }

            (message->NumSent)++;
            // unlock
            message->pathInfo.sharedmem.sharedData->fragsReadyToSend.Lock.unlock();

        }                       // end ndesc loop
    }                           // end NumFragDescAllocated < numfrags

    if(message->NumSent == message->numfrags )
	    *incomplete=false;

    // check to see if send is done (buffered sends are already "done";
    // and synchronous sends are done when the first frag is
    // acked (and everything else has been sent...)
    if (!(message->sendDone) &&
        (message->NumSent == message->numfrags) &&
        (message->requestDesc->sendType != ULM_SEND_SYNCHRONOUS)) {
        message->requestDesc->messageDone = true;
        message->sendDone = 1;
    }
    // return
    return true;
}

//! process on host frags
bool sharedmemPath::receive(double timeNow, int *errorCode,
		                recvType recvTypeArg = ALL)
{
	*errorCode=ULM_SUCCESS;
	int retCode;
	int remoteProc;
    	int comm;
       	int whichQueue;
       	int retVal = 0;
       	unsigned int sourceRank;
    	Communicator *Comm;
    	SMPFragDesc_t *incomingFrag;
	sharedmemPath sharedMemoryPathObject;

    for (remoteProc = 0; remoteProc < local_nprocs(); remoteProc++) {

        int foundData = 1;
        while (foundData) {
            foundData = 0;
            incomingFrag = (SMPFragDesc_t *) 0xbeef;
            if (usethreads()) {
                retCode =
                    SharedMemIncomingFrags[remoteProc][local_myproc
                                                       ()]->
                    readFromTail(&incomingFrag);
            } else {
                mb();
                retCode =
                    SharedMemIncomingFrags[remoteProc][local_myproc
                                                       ()]->
                    readFromTailNoLock(&incomingFrag);
                mb();
            }

            if (retCode != -1)
            {
                foundData = 1;
                // check to see if this is other than frag 0
                // new message has arrived
                // get communicator id
                comm = incomingFrag->ctx_m;

                // get source process group ProcID
                sourceRank = incomingFrag->srcProcID_m;

                // lock to prevent any other matches for this source/destination/
                //   communicator triplet (whether frags of posted receives)
                Comm = communicators[comm];
                if (usethreads())
                    Comm->recvLock[sourceRank].lock();

                /* try and make match - if match made, frag removed from the
                 *  list.  The assumption is that the recvLock is protecting the
                 *  lists from being modified, so no locking is done to mofidy
                 *  the list
                 */
                RecvDesc_t *matchedRecv =
                    Comm->checkSMPRecvListForMatch(incomingFrag,
                                                   &whichQueue);

                if (matchedRecv != (RecvDesc_t *) 0) {
                    /*
                     * Match Made
                     */

			/* this is a wider lock than really needed, but
			 *   in when request and send objects are combined,
			 *   this can be eliminated all together.
			 */
			matchedRecv->Lock.lock();
			*errorCode=sharedMemoryPathObject.processMatch
				(incomingFrag, matchedRecv);
			matchedRecv->Lock.unlock();
			if( *errorCode != ULM_SUCCESS ){
				if (usethreads())
					Comm->recvLock[sourceRank].unlock();
				return false;
			}

                } else {

                    /*
                     * no match made
                     */
                    // append on list of frgments that posted receives will match
                    //   against - need to be safe, so that other threads can
                    //   read the list in probe

                    Comm->privateQueues.OkToMatchSMPFrags[sourceRank]->
                        Append(incomingFrag);
                    // unlock triplet
                    if (usethreads())
                        Comm->recvLock[sourceRank].unlock();
                }
            }                   /* end processing frag */
        }                       /* end foundData */
    }                           /* end remoteProc loop */

    // check for frags intended for already matched messages
    while (SMPMatchedFrags[local_myproc()]->size() > 0) {
        SMPSecondFragDesc_t *incomingFrag = (SMPSecondFragDesc_t *)
            SMPMatchedFrags[local_myproc()]->GetfirstElement();

        // make sure that we really got a frag - needed for threaded
        //   use
        if (incomingFrag == SMPMatchedFrags[local_myproc()]->end()) {
            break;
        }
        //  match has already been made, and can access the matched
        //  receive directly
        RecvDesc_t *receiver =
            (RecvDesc_t *) incomingFrag->matchedRecv_m;
        BaseSendDesc_t *matchedSender =
            incomingFrag->SendingHeader_m.SMP;
        if (usethreads())
            receiver->Lock.lock();

        // copy data to destination buffers
        int done;
        int retVal =
            receiver->SMPCopyToApp(incomingFrag->seqOffset_m,
                                   incomingFrag->length_m,
                                   incomingFrag->addr_m,
                                   incomingFrag->msgLength_m, &done);
        if (retVal != ULM_SUCCESS) {
            if (usethreads())
                receiver->Lock.unlock();
	    *errorCode=retVal;
            return false;
        }
        if (receiver->DataReceived + receiver->DataInBitBucket >=
            receiver->ReceivedMessageLength) {
            // fill in request object
            receiver->requestDesc->reslts_m.proc.source_m =
                receiver->srcProcID_m;
            receiver->requestDesc->reslts_m.length_m =
                receiver->ReceivedMessageLength;
            receiver->requestDesc->reslts_m.lengthProcessed_m =
                receiver->DataReceived;
            receiver->requestDesc->reslts_m.UserTag_m =
                receiver->tag_m;
            //mark recv request as complete
            receiver->requestDesc->messageDone = true;
            wmb();
            Comm = communicators[receiver->ctx_m];
            Comm->privateQueues.MatchedRecv[receiver->srcProcID_m]->
                RemoveLink(receiver);
            if (usethreads())
                receiver->Lock.unlock();
            receiver->ReturnDesc();
        } else if (usethreads()) {
            receiver->Lock.unlock();
        }
        // return fragement to sender's freeFrags list
#ifdef _DEBUGQUEUES
	incomingFrag->WhichQueue = SMPFREELIST;
	mb();
#endif
        matchedSender->pathInfo.sharedmem.sharedData->
		freeFrags.Append(incomingFrag);

        // ack fragement
        fetchNadd((int *) &(matchedSender->pathInfo.sharedmem.sharedData->
				NumAcked), 1);

    }                           // end while( SMPMatchedFrags )

    // try and process data associated with a messages first frag
    if (firstFrags.size() > 0) {

        if (usethreads()) {
            // lock queue
            firstFrags.Lock.lock();
            for (SMPFragDesc_t * frag =
                     (SMPFragDesc_t *) firstFrags.begin();
                 frag != (SMPFragDesc_t *) firstFrags.end();
                 frag = (SMPFragDesc_t *) frag->next) {
                sharedMemData_t *matchedSender =
                    frag->SendingHeader_m.SMP;
                RecvDesc_t *matchedRecv =
                    (RecvDesc_t *) frag->matchedRecv_m;

                // check to see if this first fragement can be processes
                if (frag->okToReadPayload_m) {

                    matchedRecv->Lock.lock();
                    // copy data to destination buffers
                    int done;
                    retVal =
                        matchedRecv->SMPCopyToApp(frag->
                                                  seqOffset_m,
                                                  frag->length_m,
                                                  frag->addr_m,
                                                  frag->msgLength_m,
                                                  &done);
                    if (retVal != ULM_SUCCESS) {
                        matchedRecv->Lock.unlock();
                        firstFrags.Lock.unlock();
			*errorCode=retVal;
                        return false;
                    }
                    if (matchedRecv->DataReceived +
                        matchedRecv->DataInBitBucket >=
                        matchedRecv->ReceivedMessageLength) {
                        // fill in request object
                        matchedRecv->requestDesc->reslts_m.proc.
                            source_m = matchedRecv->srcProcID_m;
                        matchedRecv->requestDesc->reslts_m.length_m =
                            matchedRecv->ReceivedMessageLength;
                        matchedRecv->requestDesc->reslts_m.
                            lengthProcessed_m = matchedRecv->DataReceived;
                        matchedRecv->requestDesc->reslts_m.UserTag_m =
                            matchedRecv->tag_m;
                        //mark recv request as complete
                        matchedRecv->requestDesc->messageDone = true;
                        wmb();
                        Comm = communicators[matchedRecv->ctx_m];
                        Comm->privateQueues.MatchedRecv[matchedRecv->
                                                        srcProcID_m]->
                            RemoveLink(matchedRecv);
                        matchedRecv->Lock.unlock();
                        matchedRecv->ReturnDesc();
                    } else {
                        matchedRecv->Lock.unlock();
                    }

                    SMPFragDesc_t *tmp = (SMPFragDesc_t *)
                        firstFrags.RemoveLinkNoLock(frag);
		    mb();

                    // ack first frag
                    fetchNadd((int *) &(matchedSender->NumAcked), 1);

                    frag = tmp;

                }               // end if( frag->okToReadPayload )
            }

            // unlock queue
            firstFrags.Lock.unlock();
        } else {                /* no theads in use */
            // lock queue
            for (SMPFragDesc_t * frag =
                     (SMPFragDesc_t *) firstFrags.begin();
                 frag != (SMPFragDesc_t *) firstFrags.end();
                 frag = (SMPFragDesc_t *) frag->next) {
                sharedMemData_t *matchedSender =
                    frag->SendingHeader_m.SMP;
                RecvDesc_t *matchedRecv =
                    (RecvDesc_t *) frag->matchedRecv_m;

                // check to see if this first fragement can be processes
                if (frag->okToReadPayload_m) {

                    // copy data to destination buffers
                    int done;
                    retVal =
                        matchedRecv->SMPCopyToApp(frag-> seqOffset_m,
					frag->length_m, frag->addr_m,
					frag->msgLength_m, &done);
                    if (retVal != ULM_SUCCESS) {
			    *errorCode=retVal;
			    return false;
                    }
                    if (matchedRecv->DataReceived +
                        matchedRecv->DataInBitBucket >=
                        matchedRecv->ReceivedMessageLength) {
                        // fill in request object
                        matchedRecv->requestDesc->reslts_m.proc.
                            source_m = matchedRecv->srcProcID_m;
                        matchedRecv->requestDesc->reslts_m.length_m =
                            matchedRecv->ReceivedMessageLength;
                        matchedRecv->requestDesc->reslts_m.
                            lengthProcessed_m = matchedRecv->DataReceived;
                        matchedRecv->requestDesc->reslts_m.UserTag_m =
                            matchedRecv->tag_m;
                        //mark recv request as complete
                        matchedRecv->requestDesc->messageDone = true;
                        wmb();
                        Comm = communicators[matchedRecv->ctx_m];
                        Comm->privateQueues.MatchedRecv[matchedRecv->
                                                        srcProcID_m]->
                            RemoveLink(matchedRecv);
                        matchedRecv->ReturnDesc();
                    }

                    SMPFragDesc_t *tmp = (SMPFragDesc_t *)
                        firstFrags.RemoveLinkNoLock(frag);

                    // ack first frag - only one thead updating the counter
                    matchedSender->NumAcked++;

                    frag = tmp;

                }               // end if( frag->okToReadPayload )
            }

        }
    }
    /* end processing first frags */

    return true;
}

    // copy data into library buffers
    void sharedmemSendInfo::CopyToULMBuffers(SMPFragDesc_t *descriptor,
		    BaseSendDesc_t *message)
{
    SMPFragDesc_t *recvFrag = (SMPFragDesc_t *) descriptor;
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
        src_addr = ((unsigned char *) message->AppAddr) + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = recvFrag->length_m;
        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
    } else {                    // data is non-contiguous
        ULMType_t *dtype = message->datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int tm_init = recvFrag->tmapIndex_m;
        int init_cnt = recvFrag->seqOffset_m / dtype->packed_size;
        int tot_cnt = recvFrag->msgLength_m / dtype->packed_size;
        unsigned char *start_addr = ((unsigned char *) message->AppAddr)
            + init_cnt * dtype->extent;
        int dtype_cnt, ti;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size
            - tmap[tm_init].seq_offset + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size
            + tmap[tm_init].seq_offset - recvFrag->seqOffset_m;
        len_to_copy =
            (len_to_copy >
             recvFrag->length_m) ? recvFrag->length_m : len_to_copy;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = ((unsigned char *) recvFrag->addr_m)
                    + len_copied;

                len_to_copy =
                    (recvFrag->length_m - len_copied >=
                     tmap[ti].size) ? tmap[ti].size : recvFrag->length_m -
                    len_copied;
                if (len_to_copy == 0)
                    return;

                bcopy(src_addr, dest_addr, len_to_copy);
                len_copied += len_to_copy;
            }

            tm_init = 0;
            start_addr += dtype->extent;
        }
    }
}


// copy data into library buffers
void sharedmemSendInfo::CopyToULMBuffers(SMPSecondFragDesc_t * descriptor,
		BaseSendDesc_t *message)
{
    SMPSecondFragDesc_t *recvFrag = (SMPSecondFragDesc_t *) descriptor;
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
        src_addr = ((unsigned char *) message->AppAddr) + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = recvFrag->length_m;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);

    } else {                    // data is non-contiguous
        ULMType_t *dtype = message->datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int tm_init = recvFrag->tmapIndex_m;
        int init_cnt = recvFrag->seqOffset_m / dtype->packed_size;
        int tot_cnt = recvFrag->msgLength_m / dtype->packed_size;
        unsigned char *start_addr = ((unsigned char *) message->AppAddr)
            + init_cnt * dtype->extent;
        int dtype_cnt, ti;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size
            - tmap[tm_init].seq_offset + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size
            + tmap[tm_init].seq_offset - recvFrag->seqOffset_m;
        len_to_copy =
            (len_to_copy >
             recvFrag->length_m) ? recvFrag->length_m : len_to_copy;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = ((unsigned char *) recvFrag->addr_m)
                    + len_copied;

                len_to_copy =
                    (recvFrag->length_m - len_copied >=
                     tmap[ti].size) ? tmap[ti].size : recvFrag->length_m -
                    len_copied;
                if (len_to_copy == 0)
                    return;

                MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
                len_copied += len_to_copy;
            }

            tm_init = 0;
            start_addr += dtype->extent;
        }
    }
}

void sharedmemPath::ReturnDesc(BaseSendDesc_t *message, int poolIndex=-1)
{
    // sanity check - list must be empty
#ifndef _DEBUGQUEUES
    assert(message->FragsToSend.size() == 0);
    assert(message->FragsToAck.size() == 0);
#else
    if (message->FragsToSend.size() != 0L) {
        ulm_exit((-1, "sharedmemPath::ReturnDesc: message %p "
                  "FragsToSend.size() %ld numfrags %d numsent %d "
                  "numacked %d list %d\n", message, 
		  message->FragsToSend.size(),
                  message->numfrags, message->NumSent, message->NumAcked,
                  message->WhichQueue));
    }
    if (message->FragsToAck.size() != 0L) {
        ulm_exit((-1, "sharedmemPath::ReturnDesc: message %p "
                  "FragsToAck.size() %ld numfrags %d numsent %d "
                  "numacked %d list %d\n", message, message->FragsToAck.size(),
                  message->numfrags, message->NumSent, message->NumAcked,
                  message->WhichQueue));
    }
#endif                          // _DEBUGQUEUES

    // if this was a bsend (or aborted bsend), then decrement the reference
    // count for the appropriate buffer allocation
    if (message->sendType == ULM_SEND_BUFFERED) {
        if (message->PostedLength > 0) {
            ulm_bsend_decrement_refcount(
			    (ULMRequestHandle_t) message->requestDesc,
			    message->bsendOffset);
        }
    }
    // mark descriptor as beeing in the free list
    message->WhichQueue = SENDDESCFREELIST;
    // return descriptor to pool -- always freed by allocating process!
//    _ulm_SendDescriptors.returnElement(message, 0);
    //  put descriptor is cache
    sendDescCache.Append(message);
}


//! process on host frags
int sharedmemPath::processMatch(SMPFragDesc_t * incomingFrag,
		RecvDesc_t *matchedRecv)
{
	int errorCode=ULM_SUCCESS,retVal;
	int sourceRank = incomingFrag->srcProcID_m;
	int nFragsProcessed = 0;
    	int recvDone = 0;
    	Communicator *Comm;

	matchedRecv->ReceivedMessageLength=incomingFrag->msgLength_m;
#ifdef _DEBUGQUEUES
	matchedRecv->isendSeq_m = incomingFrag->isendSeq_m;
#endif                          // _DEBUGQUEUE
	matchedRecv->srcProcID_m = incomingFrag->srcProcID_m;
	matchedRecv->tag_m = incomingFrag->tag_m;

	// figure out exactly how much data will be received
	unsigned long amountToRecv = incomingFrag->msgLength_m;
	if (amountToRecv > matchedRecv->PostedLength)
		amountToRecv = matchedRecv->PostedLength;

    	matchedRecv->actualAmountToRecv_m = amountToRecv;

	// in the wild irecv, set the source process to the
	//  one from where data was actually received
	matchedRecv->srcProcID_m = incomingFrag->srcProcID_m;

	sharedMemData_t *matchedSender = incomingFrag->SendingHeader_m.SMP;

	//  1st fragment

	if (incomingFrag->length_m > 0) {
		// check to see if this first fragement can be processes
    		rmb();
    		if (incomingFrag->okToReadPayload_m) {
			// copy data to destination buffers
			retVal = matchedRecv->SMPCopyToApp(incomingFrag->
				     	seqOffset_m, incomingFrag->length_m,
					incomingFrag->addr_m,
			       		incomingFrag-> msgLength_m, &recvDone);
			if (retVal != ULM_SUCCESS) {
				errorCode=retVal;
				return errorCode;
			}
			nFragsProcessed = 1;

		} else {
			// set matched recv
			incomingFrag->matchedRecv_m = matchedRecv;

			// post on list for reading data later on
			if (usethreads())
		    		firstFrags.Append(incomingFrag);
			else
		    		firstFrags.AppendNoLock(incomingFrag);
		}
	} else {
		/* zero byte message */
		// fill in request object
    		matchedRecv->requestDesc->reslts_m.proc.source_m = sourceRank;
		matchedRecv->requestDesc->reslts_m.length_m =
			matchedRecv->ReceivedMessageLength;
	    	matchedRecv->requestDesc->reslts_m.lengthProcessed_m =
			matchedRecv->DataReceived;
	    	matchedRecv->requestDesc->reslts_m.UserTag_m = 
			matchedRecv->tag_m;
    		matchedRecv->requestDesc->messageDone = true;
    		matchedSender->NumAcked = 1;
		// return receive descriptor to pool
    		matchedRecv->ReturnDesc();
		return ULM_SUCCESS;
	}

	//   we have set matchedRecv on the receive side
	matchedSender->fragsReadyToSend.Lock.lock();

   	// mark the sender side with the address of the recv side descriptor
	matchedSender->matchedRecv=matchedRecv;

	matchedSender->fragsReadyToSend.Lock.unlock();

   	// set clear to send flag
	matchedSender->clearToSend_m = true;

	// check to see if any more frags can be processed - loop over
	//   sender's fragsReadyToSend list
	for (SMPSecondFragDesc_t * frag =
   			(SMPSecondFragDesc_t *)
		   	matchedSender->fragsReadyToSend.begin();
		       	frag != (SMPSecondFragDesc_t *)
		   	matchedSender->fragsReadyToSend.end();
		       	frag = (SMPSecondFragDesc_t *) frag->next) {

		// copy data to destination buffers
		errorCode =
	    		matchedRecv->SMPCopyToApp(frag->seqOffset_m,
					frag->length_m, frag->addr_m,
					frag-> msgLength_m, &recvDone);
		if (errorCode != ULM_SUCCESS) {
                            return errorCode;
		}
		nFragsProcessed++;

		SMPSecondFragDesc_t *tmpFrag =
	    		(SMPSecondFragDesc_t *) matchedSender->
		    	fragsReadyToSend.RemoveLinkNoLock(frag);

		// return fragement to sender's freeFrags list
#ifdef _DEBUGQUEUES
		frag->WhichQueue = SMPFREELIST;
		mb();
#endif                          /* _DEBUGQUEUE */
		matchedSender->freeFrags.Append(frag);

		frag = tmpFrag;
    	}

	if (matchedRecv->DataReceived +
			matchedRecv->DataInBitBucket >=
			matchedRecv->ReceivedMessageLength) {
		// fill in request object
		matchedRecv->requestDesc->reslts_m.proc.
	    		source_m = matchedRecv->srcProcID_m;
		matchedRecv->requestDesc->reslts_m.length_m =
	    		matchedRecv->ReceivedMessageLength;
		matchedRecv->requestDesc->reslts_m.lengthProcessed_m = 
			matchedRecv->DataReceived;
		matchedRecv->requestDesc->reslts_m.UserTag_m =
	    		matchedRecv->tag_m;
		//mark recv request as complete
		matchedRecv->requestDesc->messageDone = true;
		wmb();
		matchedRecv->ReturnDesc();
    	} else {
		//  add this descriptor to the matched ireceive list
                Comm = communicators[incomingFrag->ctx_m];
		matchedRecv->WhichQueue = MATCHEDIRECV;
		Comm->privateQueues.MatchedRecv[sourceRank]->
	    		Append(matchedRecv);
	}

	// ack fragement
	fetchNadd((int *) &(matchedSender->NumAcked),nFragsProcessed);

    return errorCode;
}
