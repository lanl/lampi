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

#if ENABLE_SHARED_MEMORY
# include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/sharedmem/path.h"
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
#if ENABLE_SHARED_MEMORY
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
#if ENABLE_SHARED_MEMORY
        if (matchAgainstSMPFragList(IRDesc, ProcWithData)) {
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
    if (usethreads()) {
        // must use list thread lock instead of recvLock[src]
        // since we must protect the list when multiple
        // incoming fragments (from different sources) are
        // being processed simultaneously
        privateQueues.PostedWildRecv.Append(IRDesc);
    }
    else {
        privateQueues.PostedWildRecv.AppendNoLock(IRDesc);
    }

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
    int SourceProc = IRDesc->posted_m.peer_m;

#if ENABLE_SHARED_MEMORY
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
#if ENABLE_SHARED_MEMORY
    if (matchAgainstSMPFragList(IRDesc, SourceProc)) {
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
// it places the IRDesc in the appropriate matched receive list.  It also
// moves Recv descriptors among appropriate lists...recvLock[ProcWithData]
// must be held for multi-threaded operation!
//

bool Communicator::checkSpecifiedFragListsForMatch(RecvDesc_t * IRDesc,
                                                   int ProcWithData)
{
    bool FragFound = false, recvDone;
    unsigned long SendingSequenceNumber = 0;
    long SendingProc = 0;
    int tag = IRDesc->posted_m.tag_m;
    RequestDesc_t *requestDesc = (RequestDesc_t *) IRDesc;
    // loop over list of frags - upper level manages thread safety

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
            IRDesc->reslts_m.length_m = RecDesc->msgLength_m;
            /* reset tag variable to matched value for further list processing */
            IRDesc->reslts_m.tag_m = tag = RecDesc->tag_m;
            IRDesc->reslts_m.peer_m = SendingProc;

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
            if (requestDesc->messageDone==REQUEST_COMPLETE) {
                break;
            } else {
                //
                // Record this irecv in the list of matched irecvs.
                //
                IRDesc->WhichQueue = MATCHEDIRECV;
                privateQueues.MatchedRecv[IRDesc->reslts_m.peer_m]->
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
            if (recvDone) {
                assert(requestDesc->messageDone != REQUEST_COMPLETE);
                requestDesc->messageDone = REQUEST_COMPLETE;
            }

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
            if (requestDesc->messageDone==REQUEST_COMPLETE) {
                break;
            }
        }
    }

    return FragFound;
}


#if ENABLE_SHARED_MEMORY

//
// this routine tries to match the receiver.  If a match is found, it
// places the receiver in the appropriate matched receive list.  It
// also moves Recv descriptors among appropriate lists
//

bool Communicator::matchAgainstSMPFragList(RecvDesc_t * receiver,
                                              int sourceProcess)
{
    bool FragFound = false;
    int errorCode;
    sharedmemPath sharedMemObject;

    // return if nothing to get
    if (privateQueues.OkToMatchSMPFrags[sourceProcess]->size() == 0) {
        return FragFound;
    }

    if (usethreads()) {
        receiver->Lock.lock();
    }

    int tag = receiver->posted_m.tag_m;
    // loop over list of frags - upper level manages thread safety


    for (SMPFragDesc_t *frag = (SMPFragDesc_t *)privateQueues.
         OkToMatchSMPFrags[sourceProcess]->begin();
         frag != (SMPFragDesc_t *) privateQueues.
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
		    errorCode=sharedMemObject.processMatch(frag,receiver);

            // break - match found
            break;

        }                   // end of matched region
    }

    if (usethreads()) {
        receiver->Lock.unlock();
    }

    return FragFound;
}

#endif                          // SHARED_MEMORY
