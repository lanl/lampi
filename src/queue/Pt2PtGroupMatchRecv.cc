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

#ifdef SHARED_MEMORY
# include "path/sharedmem/SMPSendDesc.h"
# include "path/sharedmem/SMPSharedMemGlobals.h"
#endif // SHARED_MEMORY

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "queue/globals.h"
#include "internal/profiler.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

//
// this routine is used to try and match a wild posted irec - where
// wild is determined by the value assigned to the source process
//
void Communicator::checkFragListsForWildMatch(RecvDesc_t * IRDesc)
{
    //
    // Loop over all the outstanding messages to find one that matches.
    // There is an outer loop over lists of messages from each
    // processor, then an inner loop over the messages from the
    // processor.
    //
    for (int ProcWithData = 0; ProcWithData < remoteGroup->groupSize;
         ProcWithData++) {

        // continue if no frags to match
#ifdef SHARED_MEMORY
        if ((privateQueues.OkToMatchRecvFrags[ProcWithData]->size() == 0)
            && (privateQueues.OkToMatchSMPFrags[ProcWithData]->size() ==
                0)) {
            continue;
        }
#else
        if (privateQueues.OkToMatchRecvFrags[ProcWithData]->size() == 0)
            continue;
#endif                          // SHARED_MEMORY
        //
        // loop over messages from the current processor.
        //
        if (checkSpecifiedFragListsForMatch(IRDesc, ProcWithData)) {
            // match found
            return;
        }
#ifdef SHARED_MEMORY
        if (matchAgainstSMPFramentList(IRDesc, ProcWithData)) {
            // match found
            return;
        }
#endif                          // SHARED_MEMORY
    }                           // end of ProcWithData loop

    //
    // We didn't find any matches.  Record this irecv so we can match to
    // it when the message comes in.
    //
    IRDesc->WhichQueue = POSTEDWILDIRECV;
    privateQueues.PostedWildRecv.AppendNoLock(IRDesc);

    //
    return;
}


//
// This routine is used to match a posted irecv.  The source process is
// specified.
//

void Communicator::checkFragListsForSpecificMatch(RecvDesc_t * IRDesc)
{
    // check to see if there are any frags to match
    int SourceProc = IRDesc->srcProcID_m;

#ifdef SHARED_MEMORY
    if ((privateQueues.OkToMatchRecvFrags[SourceProc]->size() == 0) &&
        (privateQueues.OkToMatchSMPFrags[SourceProc]->size() == 0)) {
        IRDesc->WhichQueue = POSTEDIRECV;
        privateQueues.PostedSpecificRecv[SourceProc]->AppendNoLock(IRDesc);
        return;
    }
#else
    if (privateQueues.OkToMatchRecvFrags[SourceProc]->size() == 0) {
        IRDesc->WhichQueue = POSTEDIRECV;
        privateQueues.PostedSpecificRecv[SourceProc]->AppendNoLock(IRDesc);
        return;
    }
#endif                          // SHARED_MEMORY
    //
    // Try to match this tag and proc to the list of outstanding frags
    // from that proc.
    //
    if (checkSpecifiedFragListsForMatch(IRDesc, SourceProc)) {
        // match found
        return;
    }
#ifdef SHARED_MEMORY
    if (matchAgainstSMPFramentList(IRDesc, SourceProc)) {
        // match found
        return;
    }
#endif                          // SHARED_MEMORY
    // no match found
    IRDesc->WhichQueue = POSTEDIRECV;
    privateQueues.PostedSpecificRecv[SourceProc]->AppendNoLock(IRDesc);

    return;
}


//
// this routine tries to match the posted IRecv.  If a match is found,
//  it places the IRDesc in the appropriate matched receive list.  It also
//  moves Recv descriptors among appropriate lists
//

bool Communicator::checkSpecifiedFragListsForMatch(RecvDesc_t * IRDesc,
                                                   int ProcWithData)
{
    bool FragFound = false, recvDone;
    unsigned long SendingSequenceNumber = 0;
    unsigned long SendingProc = 0;
    int tag = IRDesc->tag_m;
    RequestDesc_t *requestDesc = IRDesc->requestDesc;
    // loop over list of frags - upper level manages thread safety

    // lock for thread safety
    if (usethreads())
        privateQueues.OkToMatchRecvFrags[ProcWithData]->Lock.lock();

    for (BaseRecvFragDesc_t *
         RecDesc =
         (BaseRecvFragDesc_t *) privateQueues.
         OkToMatchRecvFrags[ProcWithData]->begin();
         RecDesc !=
         (BaseRecvFragDesc_t *) privateQueues.
         OkToMatchRecvFrags[ProcWithData]->end();
         RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
        // pull off first frag - we assume that process matching has been
        //   done already
        if ((((tag == ULM_ANY_TAG) || (tag == RecDesc->tag_m)))
            && !FragFound) {
            if (tag == ULM_ANY_TAG && RecDesc->tag_m < 0) {
                continue;
            }
            //
            // Pull out the proc this message is really from.
            //
            //
            SendingProc = RecDesc->srcProcID_m;

            FragFound = true;
            SendingSequenceNumber = RecDesc->isendSeq_m;
            IRDesc->isendSeq_m = SendingSequenceNumber;
            IRDesc->ReceivedMessageLength = RecDesc->msgLength_m;
            // figure out exactly how much data will be received
            unsigned long amountToRecv = RecDesc->msgLength_m;
            if (amountToRecv > IRDesc->PostedLength)
                amountToRecv = IRDesc->PostedLength;
            IRDesc->actualAmountToRecv_m = amountToRecv;

            IRDesc->tag_m = RecDesc->tag_m;
            IRDesc->srcProcID_m = SendingProc;

            //
            // Copy the data to user space and record the amount moved.
            //

            // copyout data
            // CopyToApp will return >= 0 if the data is okay, and -1
            // if the data is corrupted -- we don't care since we
            // call AckData in either case...
            IRDesc->CopyToApp(RecDesc);

            // remove descriptor from unmatched list - RecDesc now points to
            //  previous element, so don't refernce any more to manipulate
            //  data associated with the match

            //  ReturnDescToPool sets the WhichQueue correctly - this is just to
            //    make sure that the frag is not processed as privateQueues.OkToMatchRecvFrags.
            RecDesc->WhichQueue = FRAGSTOACK;
            BaseRecvFragDesc_t *TmpDesc = RecDesc;
            RecDesc = (BaseRecvFragDesc_t *)
                privateQueues.OkToMatchRecvFrags[ProcWithData]->
                RemoveLinkNoLock(TmpDesc);

            // send ack
            bool acked = TmpDesc->AckData();
            if (acked) {
                // return descriptor to pool
                TmpDesc->ReturnDescToPool(getMemPoolIndex());
            } else {
                // move descriptor to list of unsent acks
                UnprocessedAcks.Append(TmpDesc);
            }

            // CopyToApp() may have freed IRDesc, so we use requestDesc
            // to check the status of this message...
            if (requestDesc->messageDone) {
                break;
            } else {
                //
                // Record this irecv in the list of matched irecvs.
                //
                IRDesc->WhichQueue = MATCHEDIRECV;
                privateQueues.MatchedRecv[IRDesc->srcProcID_m]->
                    Append(IRDesc);
            }

        } else if (FragFound
                   && (RecDesc->isendSeq_m == SendingSequenceNumber)
                   && (RecDesc->srcProcID_m == SendingProc)
                   && (RecDesc->tag_m == tag)) {

            //
            // Copy the data to user space and record the amount moved.
            //

            // copy out data
            // CopyToAppLock will return >= 0 if data is okay, and -1 if it is corrupt;
            // we don't care either way since we call AckData in either case
            recvDone = false;
            IRDesc->CopyToAppLock(RecDesc, &recvDone);
            if (recvDone)
                requestDesc->messageDone = true;

            //  ReturnDescToPool sets the WhichQueue correctly - this is just to
            //    make sure that the frag is not processed as privateQueues.OkToMatchRecvFrags.
            RecDesc->WhichQueue = FRAGSTOACK;

            // remove descriptor from unmatched list - RecDesc now points to
            //  previous element, so don't refernce any more to manipulate
            //  data associated with the match
            BaseRecvFragDesc_t *TmpDesc = RecDesc;
            RecDesc = (BaseRecvFragDesc_t *)
                privateQueues.OkToMatchRecvFrags[ProcWithData]->
                RemoveLinkNoLock(RecDesc);

            // send ack
            if (TmpDesc->AckData()) {
                // return descriptor to pool
                TmpDesc->ReturnDescToPool(getMemPoolIndex());
            } else {
                // move descriptor to list of unsent acks
                UnprocessedAcks.Append(TmpDesc);
            }

            // CopyToApp() may have freed IRDesc, so we use requestDesc
            // to check the status of this message...
            if (requestDesc->messageDone) {
                break;
            }
        }
    }

    // unlock
    if (usethreads())
        privateQueues.OkToMatchRecvFrags[ProcWithData]->Lock.unlock();

    return FragFound;
}


#ifdef SHARED_MEMORY

//
// this routine tries to match the receiver.  If a match is found, it
// places the receiver in the appropriate matched receive list.  It
// also moves Recv descriptors among appropriate lists
//

bool Communicator::matchAgainstSMPFramentList(RecvDesc_t * receiver,
                                              int sourceProcess)
{
    bool FragFound = false;
    // return if nothing to get
    if (privateQueues.OkToMatchSMPFrags[sourceProcess]->size() == 0) {
        return FragFound;
    }
    int tag = receiver->tag_m;
    // loop over list of frags - upper level manages thread safety

    if (usethreads()) {
        // lock for thread safety
        privateQueues.OkToMatchSMPFrags[sourceProcess]->Lock.lock();
        SMPFragDesc_t *Next = (SMPFragDesc_t *)
            privateQueues.OkToMatchSMPFrags[sourceProcess]->begin();
        Next = (SMPFragDesc_t *) Next->next;
        for (SMPFragDesc_t *frag =
                 (SMPFragDesc_t *)privateQueues.
                 OkToMatchSMPFrags[sourceProcess]->begin();
             frag !=
                 (SMPFragDesc_t *) privateQueues.
                 OkToMatchSMPFrags[sourceProcess]->end();
             frag = (SMPFragDesc_t *) frag->next) {

            // pull off first frag, only 0th frag will be present if
            //   match has not yet been made.
            if ((tag == ULM_ANY_TAG) || (tag == frag->tag_m)) {
                if (tag == ULM_ANY_TAG && frag->tag_m < 0) {
                    continue;
                }

                // remove frag from list
                privateQueues.OkToMatchSMPFrags[sourceProcess]->
                    RemoveLinkNoLock(frag);

                FragFound = true;

                receiver->ReceivedMessageLength = frag->msgLength_m;
#ifdef _DEBUGQUEUE
                receiver->isendSeq_m = frag->isendSeq_m;
#endif                          // _DEBUGQUEUE
                receiver->srcProcID_m = frag->srcProcID_m;
                receiver->tag_m = frag->tag_m;

                // figure out exactly how much data will be received
                unsigned long amountToRecv = frag->msgLength_m;
                if (amountToRecv > receiver->PostedLength)
                    amountToRecv = receiver->PostedLength;

                // process data
                receiver->actualAmountToRecv_m = amountToRecv;
                SMPSendDesc_t *matchedSender = frag->SendingHeader_m.SMP;

                if (matchedSender->NumSent == matchedSender->numfrags) {
                    //
                    // all message has been sent
                    //

                    // length > 0
                    rmb();
                    if (frag->length_m > 0) {

                        // copy data to destination buffers
                        int recvDone;
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            privateQueues.
                                OkToMatchSMPFrags[sourceProcess]->Lock.
                                unlock();
                            return retVal;
                        }

                    }
                    // check to see if any more frags can be processed - loop over
                    //   sender's fragsReadyToSend list
                    if (frag->msgLength_m > SMPFirstFragPayload) {
                        for (SMPSecondFragDesc_t * frag =
                                 (SMPSecondFragDesc_t *)
                                 matchedSender->fragsReadyToSend.begin();
                             frag != (SMPSecondFragDesc_t *)
                                 matchedSender->fragsReadyToSend.end();
                             frag = (SMPSecondFragDesc_t *) frag->next) {

                            // copy data to destination buffers
                            int recvDone;
                            int retVal =
                                receiver->SMPCopyToApp(frag->seqOffset_m,
                                                       frag->length_m,
                                                       frag->addr_m,
                                                       frag->msgLength_m,
                                                       &recvDone);
                            if (retVal != ULM_SUCCESS) {
                                privateQueues.
                                    OkToMatchSMPFrags[sourceProcess]->Lock.
                                    unlock();
                                return retVal;
                            }
                            // remove frag
                            SMPSecondFragDesc_t *tmp =
                                (SMPSecondFragDesc_t *)
                                matchedSender->fragsReadyToSend.
                                RemoveLinkNoLock(frag);

                            // return frag to sender's freeFrags list
                            matchedSender->freeFrags.AppendNoLock(frag);
                            frag = tmp;

                        }
                    }

                    /* return descriptor to pool, if all data has been received
                     */
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
                    receiver->ReturnDesc();

                    // since this is the only frag, no one else will
                    //  be writing to the request object
                    matchedSender->NumAcked = matchedSender->NumSent;

                } else {
                    int recvDone = 0;
                    receiver->Lock.lock();
                    // only part of message is being received
                    // mark the sender side with the address of the recv side descriptor
                    frag->SendingHeader_m.SMP->fragsReadyToSend.Lock.
                        lock();
                    frag->SendingHeader_m.SMP->matchedRecv = receiver;
                    frag->SendingHeader_m.SMP->fragsReadyToSend.Lock.
                        unlock();
                    matchedSender->clearToSend_m = true;

                    int fragsProcessed = 0;

                    // check to see if this first frag can be processes
                    rmb();
                    if (frag->okToReadPayload_m) {

                        // copy data to destination buffers
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            privateQueues.
                                OkToMatchSMPFrags[sourceProcess]->Lock.
                                unlock();
                            receiver->Lock.unlock();
                            return retVal;
                        }
                        // ack first frag
                        fragsProcessed++;
                    } else {
                        frag->matchedRecv_m = receiver;
                        firstFrags.Append(frag);
                    }

                    // check to see if any more frags can be processed - loop over
                    //   sender's fragsReadyToSend list
                    for (SMPSecondFragDesc_t * frag =
                             (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.begin();
                         frag != (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.end();
                         frag = (SMPSecondFragDesc_t *) frag->next) {

                        // copy data to destination buffers
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            privateQueues.
                                OkToMatchSMPFrags[sourceProcess]->Lock.
                                unlock();
                            receiver->Lock.unlock();
                            return retVal;
                        }
                        fragsProcessed++;

                        // remove frag
                        SMPSecondFragDesc_t *tmp =
                            (SMPSecondFragDesc_t *)
                            matchedSender->fragsReadyToSend.
                            RemoveLinkNoLock(frag);

                        // return frag to sender's freeFrags list
                        matchedSender->freeFrags.Append(frag);

                        frag = tmp;
                    }

                    if (receiver->DataReceived +
                        receiver->DataInBitBucket >=
                        receiver->ReceivedMessageLength) {
                        // fill in request object
                        receiver->requestDesc->reslts_m.proc.source_m =
                            receiver->srcProcID_m;
                        receiver->requestDesc->reslts_m.length_m =
                            receiver->ReceivedMessageLength;
                        receiver->requestDesc->reslts_m.
                            lengthProcessed_m = receiver->DataReceived;
                        receiver->requestDesc->reslts_m.UserTag_m =
                            receiver->tag_m;
                        //mark recv request as complete
                        receiver->requestDesc->messageDone = true;
                        wmb();
                        receiver->Lock.unlock();
                        receiver->ReturnDesc();
                    } else {
                        // append descriptor to matched list
                        privateQueues.MatchedRecv[sourceProcess]->
                            Append(receiver);
                        receiver->Lock.unlock();
                    }
                    // ack frag
                    if (fragsProcessed > 0) {
                        fetchNadd((int *) &(matchedSender->NumAcked),
                                  fragsProcessed);
                    }

                }

                // break - match found
                break;

            }                   // end of matched region
        }

        // unlock
        privateQueues.OkToMatchSMPFrags[sourceProcess]->Lock.unlock();
    } else {

        SMPFragDesc_t *Next = (SMPFragDesc_t *)
            privateQueues.OkToMatchSMPFrags[sourceProcess]->begin();
        Next = (SMPFragDesc_t *) Next->next;
        for (SMPFragDesc_t *frag =
                 (SMPFragDesc_t *)privateQueues.
                 OkToMatchSMPFrags[sourceProcess]->begin();
             frag !=
                 (SMPFragDesc_t *) privateQueues.
                 OkToMatchSMPFrags[sourceProcess]->end();
             frag = (SMPFragDesc_t *) frag->next) {

            // pull off first frag, only 0th frag will be present if
            //   match has not yet been made.
            if ((tag == ULM_ANY_TAG) || (tag == frag->tag_m)) {
                if (tag == ULM_ANY_TAG && frag->tag_m < 0) {
                    continue;
                }

                // remove frag from list
                privateQueues.OkToMatchSMPFrags[sourceProcess]->
                    RemoveLinkNoLock(frag);

                FragFound = true;

                receiver->ReceivedMessageLength = frag->msgLength_m;
#ifdef _DEBUGQUEUE
                receiver->isendSeq_m = frag->isendSeq_m;
#endif                          // _DEBUGQUEUE
                receiver->srcProcID_m = frag->srcProcID_m;
                receiver->tag_m = frag->tag_m;

                // figure out exactly how much data will be received
                unsigned long amountToRecv = frag->msgLength_m;
                if (amountToRecv > receiver->PostedLength)
                    amountToRecv = receiver->PostedLength;

                // process data
                receiver->actualAmountToRecv_m = amountToRecv;
                SMPSendDesc_t *matchedSender = frag->SendingHeader_m.SMP;

                if (matchedSender->NumSent == matchedSender->numfrags) {
                    //
                    // all message has been sent
                    //

                    // length > 0
                    rmb();
                    if (frag->length_m > 0) {

                        // copy data to destination buffers
                        int recvDone;
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            return retVal;
                        }
                    }
                    // check to see if any more frags can be processed - loop over
                    //   sender's fragsReadyToSend list
                    if (frag->msgLength_m > SMPFirstFragPayload) {
                        for (SMPSecondFragDesc_t * frag =
                                 (SMPSecondFragDesc_t *)
                                 matchedSender->fragsReadyToSend.begin();
                             frag != (SMPSecondFragDesc_t *)
                                 matchedSender->fragsReadyToSend.end();
                             frag = (SMPSecondFragDesc_t *) frag->next) {

                            // copy data to destination buffers
                            int recvDone;
                            int retVal =
                                receiver->SMPCopyToApp(frag->seqOffset_m,
                                                       frag->length_m,
                                                       frag->addr_m,
                                                       frag->msgLength_m,
                                                       &recvDone);
                            if (retVal != ULM_SUCCESS) {
                                return retVal;
                            }
                            // remove frag
                            SMPSecondFragDesc_t *tmp =
                                (SMPSecondFragDesc_t *)
                                matchedSender->fragsReadyToSend.
                                RemoveLinkNoLock(frag);

                            // return frag to sender's freeFrags list
                            matchedSender->freeFrags.AppendNoLock(frag);

                            frag = tmp;

                        }
                    }
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
                    receiver->ReturnDesc();

                    // since this is the only frag, no one else will
                    //  be writing to the request object
                    matchedSender->NumAcked = matchedSender->NumSent;

                } else {
                    int recvDone = 0;
                    // only part of message is being received
                    // mark the sender side with the address of the recv side descriptor
                    frag->SendingHeader_m.SMP->fragsReadyToSend.Lock.lock();
                    frag->SendingHeader_m.SMP->matchedRecv = receiver;
                    frag->SendingHeader_m.SMP->fragsReadyToSend.Lock.unlock();

                    matchedSender->clearToSend_m = true;

                    int fragsProcessed = 0;
                    // check to see if this first frag can be processes
                    rmb();
                    if (frag->okToReadPayload_m) {

                        // copy data to destination buffers
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            return retVal;
                        }
                        // ack first frag
                        fragsProcessed++;
                    } else {
                        frag->matchedRecv_m = receiver;
                        firstFrags.AppendNoLock(frag);
                    }

                    // check to see if any more frags can be processed - loop over
                    //   sender's fragsReadyToSend list
                    for (SMPSecondFragDesc_t * frag =
                             (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.begin();
                         frag != (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.end();
                         frag = (SMPSecondFragDesc_t *) frag->next) {

                        // copy data to destination buffers
                        int retVal =
                            receiver->SMPCopyToApp(frag->seqOffset_m,
                                                   frag->length_m,
                                                   frag->addr_m,
                                                   frag->msgLength_m,
                                                   &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            return retVal;
                        }
                        // remove frag
                        SMPSecondFragDesc_t *tmp =
                            (SMPSecondFragDesc_t *)
                            matchedSender->fragsReadyToSend.
                            RemoveLinkNoLock(frag);

                        // return frag to sender's freeFrags list
                        matchedSender->freeFrags.Append(frag);

                        // ack frag
                        fragsProcessed++;
                        frag = tmp;
                    }
                    if (receiver->DataReceived +
                        receiver->DataInBitBucket >=
                        receiver->ReceivedMessageLength) {
                        // fill in request object
                        receiver->requestDesc->reslts_m.proc.source_m =
                            receiver->srcProcID_m;
                        receiver->requestDesc->reslts_m.length_m =
                            receiver->ReceivedMessageLength;
                        receiver->requestDesc->reslts_m.
                            lengthProcessed_m = receiver->DataReceived;
                        receiver->requestDesc->reslts_m.UserTag_m =
                            receiver->tag_m;
                        //mark recv request as complete
                        receiver->requestDesc->messageDone = true;
                        wmb();
                        receiver->ReturnDesc();
                    } else {
                        // append descriptor to matched list
                        privateQueues.MatchedRecv[sourceProcess]->
                            AppendNoLock(receiver);
                    }
                    matchedSender->NumAcked = fragsProcessed;

                }

                // break - match found
                break;

            }                   // end of matched region
        }
    }

    return FragFound;
}

#endif                          // SHARED_MEMORY
