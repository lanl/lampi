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
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "internal/state.h"
#include "os/atomic.h"          /* for fetchNadd */
#include "ulm/ulm.h"

//! process on host frags

int processSMPFrags()
{
    int returnValue = ULM_SUCCESS;
    int retCode;
    int remoteProc;
    int comm;
    int whichQueue;
    int recvDone = 0;
    int retVal = 0;
    unsigned long amountToRecv;
    unsigned int sourceRank;
    Communicator *Comm;
    SMPFragDesc_t *incomingFrag;
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

                    matchedRecv->ReceivedMessageLength =
                        incomingFrag->msgLength_m;
#ifdef _DEBUGQUEUE
                    matchedRecv->isendSeq_m = incomingFrag->isendSeq_m;
#endif                          // _DEBUGQUEUE
                    matchedRecv->srcProcID_m =
                        incomingFrag->srcProcID_m;
                    matchedRecv->tag_m = incomingFrag->tag_m;

                    // figure out exactly how much data will be received
                    amountToRecv = incomingFrag->msgLength_m;
                    if (amountToRecv > matchedRecv->PostedLength)
                        amountToRecv = matchedRecv->PostedLength;

                    matchedRecv->actualAmountToRecv_m = amountToRecv;

                    // in the wild irecv, set the source process to the
                    //  one from where data was actually received
                    matchedRecv->srcProcID_m =
                        incomingFrag->srcProcID_m;

                    SMPSendDesc_t *matchedSender =
                        incomingFrag->SendingHeader_m.SMP;

                    //  1 frag message
                    if (incomingFrag->msgLength_m <= SMPFirstFragPayload) {
                        // unlock triplet
                        if (usethreads())
                            Comm->recvLock[sourceRank].unlock();

                        if (incomingFrag->length_m > 0) {
                            // check to see if this first fragement can be processes
                            rmb();
                            if (incomingFrag->okToReadPayload_m) {

                                // copy data to destination buffers
                                retVal =
                                    matchedRecv->
                                    SMPCopyToApp(incomingFrag->
                                                 seqOffset_m,
                                                 incomingFrag->length_m,
                                                 incomingFrag->addr_m,
                                                 incomingFrag->
                                                 msgLength_m, &recvDone);
                                if (retVal != ULM_SUCCESS) {
                                    return retVal;
                                }
                                // fill in request object
                                matchedRecv->requestDesc->reslts_m.proc.
                                    source_m = matchedRecv->srcProcID_m;
                                matchedRecv->requestDesc->reslts_m.
                                    length_m =
                                    matchedRecv->ReceivedMessageLength;
                                matchedRecv->requestDesc->reslts_m.
                                    lengthProcessed_m =
                                    matchedRecv->DataReceived;
                                matchedRecv->requestDesc->reslts_m.
                                    UserTag_m = matchedRecv->tag_m;
                                //mark recv request as complete
                                matchedRecv->requestDesc->messageDone =
                                    true;
                                wmb();
                                matchedRecv->ReturnDesc();

                                // ack first frag
                                matchedSender->NumAcked = 1;

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
                            // fill in request object
                            matchedRecv->requestDesc->reslts_m.proc.
                                source_m = sourceRank;
                            matchedRecv->requestDesc->reslts_m.length_m =
                                matchedRecv->ReceivedMessageLength;
                            matchedRecv->requestDesc->reslts_m.
                                lengthProcessed_m =
                                matchedRecv->DataReceived;
                            matchedRecv->requestDesc->reslts_m.
                                UserTag_m = matchedRecv->tag_m;
                            matchedRecv->requestDesc->messageDone = true;
                            matchedSender->NumAcked = 1;
                            // return receive descriptor to pool
                            matchedRecv->ReturnDesc();
                        }
                        continue;
                    }
                    // end 1 frag section
                    //
                    // lock matchedRecv to avoid race conditions w/o threads
                    if (usethreads())
                        matchedRecv->Lock.lock();

                    // lock the sender to make sure that NumSent does not change until
                    //   we have set matchedRecv on the receive side
                    matchedSender->fragsReadyToSend.Lock.lock();

                    // mark the sender side with the address of the recv side descriptor
                    matchedSender->matchedRecv = matchedRecv;

                    matchedSender->fragsReadyToSend.Lock.unlock();

                    // unlock triplet
                    if (usethreads())
                        Comm->recvLock[sourceRank].unlock();

                    // set clear to send flag
                    matchedSender->clearToSend_m = true;


                    //
                    // process data
                    //
                    int nFragsProcessed = 0;

                    // check to see if this first fragement can be processes
                    if (incomingFrag->okToReadPayload_m) {

                        // copy data to destination buffers
                        retVal =
                            matchedRecv->SMPCopyToApp(incomingFrag->
                                                      seqOffset_m,
                                                      incomingFrag->
                                                      length_m,
                                                      incomingFrag->addr_m,
                                                      incomingFrag->
                                                      msgLength_m,
                                                      &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            if (usethreads())
                                matchedRecv->Lock.unlock();
                            return retVal;
                        }
                        nFragsProcessed++;

                    } else {

                        // set matched recv
                        incomingFrag->matchedRecv_m = matchedRecv;

                        // post on list for reading data later on
                        if (usethreads())
                            firstFrags.Append(incomingFrag);
                        else
                            firstFrags.AppendNoLock(incomingFrag);
                    }

                    // check to see if any more frags can be processed - loop over
                    //   sender's fragsReadyToSend list
                    for (SMPSecondFragDesc_t * frag =
                             (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.begin();
                         frag != (SMPSecondFragDesc_t *)
                             matchedSender->fragsReadyToSend.end();
                         frag =
                             (SMPSecondFragDesc_t *) frag->next) {

                        // copy data to destination buffers
                        retVal =
                            matchedRecv->SMPCopyToApp(frag->
                                                      seqOffset_m,
                                                      frag->length_m,
                                                      frag->addr_m,
                                                      frag->
                                                      msgLength_m,
                                                      &recvDone);
                        if (retVal != ULM_SUCCESS) {
                            if (usethreads())
                                matchedRecv->Lock.unlock();
                            return retVal;
                        }
                        nFragsProcessed++;

                        SMPSecondFragDesc_t *tmpFrag =
                            (SMPSecondFragDesc_t *) matchedSender->
                            fragsReadyToSend.RemoveLinkNoLock(frag);

                        // return fragement to sender's freeFrags list
#ifdef _DEBUGQUEUE
                        incomingFrag->WhichQueue = SMPFREELIST;
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
                        matchedRecv->requestDesc->reslts_m.
                            lengthProcessed_m = matchedRecv->DataReceived;
                        matchedRecv->requestDesc->reslts_m.UserTag_m =
                            matchedRecv->tag_m;
                        //mark recv request as complete
                        matchedRecv->requestDesc->messageDone = true;
                        if (usethreads())
                            matchedRecv->Lock.unlock();
                        wmb();
                        matchedRecv->ReturnDesc();
                    } else {
                        //  add this descriptor to the matched ireceive list
                        matchedRecv->WhichQueue = MATCHEDIRECV;
                        Comm->privateQueues.MatchedRecv[sourceRank]->
                            Append(matchedRecv);
                        if (usethreads())
                            matchedRecv->Lock.unlock();
                    }

                    // ack fragement
                    fetchNadd((int *) &(matchedSender->NumAcked),
                              nFragsProcessed);

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
        SMPSendDesc_t *matchedSender =
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
            return retVal;
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
        matchedSender->freeFrags.Append(incomingFrag);

        // ack fragement
        fetchNadd((int *) &(matchedSender->NumAcked), 1);

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
                SMPSendDesc_t *matchedSender =
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
                        return retVal;
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
                SMPSendDesc_t *matchedSender =
                    frag->SendingHeader_m.SMP;
                RecvDesc_t *matchedRecv =
                    (RecvDesc_t *) frag->matchedRecv_m;

                // check to see if this first fragement can be processes
                if (frag->okToReadPayload_m) {

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
                        return retVal;
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

    return returnValue;
}
