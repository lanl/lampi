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



#include "queue/Communicator.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "queue/globals.h"
#include "queue/globals.h"
#include "internal/buffer.h"
#include "internal/log.h"
#include "os/atomic.h"

//!
//! try and match frag to posted receives
//! The calling routine garantees that no other thread
//!   will access the posted receive queues while this
//!   routine is being executed.
//!

RecvDesc_t *Communicator::checkSMPRecvListForMatch
    (SMPFragDesc_t * RecvDesc, int *queueMatched) {
    RecvDesc_t *ReturnValue = 0;

    //
    // Get begin pointers for both the specific and the wild list of
    // irecvs.  We'll be looping through both sets of irecvs
    // simultaneously, always incrementing the older one in order to
    // preserve message ordering.
    //
    // We'll be deleting an entry from wild_irecvs or specific_irecvs if
    // we find a match, so carry around the pointer to the previous
    // element of the slists.
    //
    int SourceProcess = RecvDesc->srcProcID_m;

    // must use list thread lock instead of recvLock[src]
    // since multiple threads may be processing incoming 
    // fragments (from different sources) simultaneously
    if (usethreads()) {
        privateQueues.PostedWildRecv.Lock.lock();
    }

    if (privateQueues.PostedSpecificRecv[SourceProcess]->size() == 0) {
        //
        // There are only wild irecvs, so specialize the algorithm.
        //
        ReturnValue = checkSMPWildRecvListForMatch(RecvDesc);
        *queueMatched = Communicator::WILD_RECV_QUEUE;
    } else if (privateQueues.PostedWildRecv.size() == 0) {
        //
        // There are only specific irecvs, so specialize the algorithm.
        //
        ReturnValue = checkSMPSpecificRecvListForMatch(RecvDesc);
        *queueMatched = Communicator::SPECIFIC_RECV_QUEUE;
    } else {
        //
        // There are some of each.
        //
        ReturnValue =
            checkSMPSpecificAndWildRecvListForMatch(RecvDesc,
                                                    queueMatched);
    }

    if (usethreads()) {
        privateQueues.PostedWildRecv.Lock.unlock();
    }

    return ReturnValue;
}

//!

RecvDesc_t *Communicator::
checkSMPWildRecvListForMatch(SMPFragDesc_t * incomingFrag)
{
    RecvDesc_t *ReturnValue = 0;
    //
    // Loop over the wild irecvs.
    //
    int FragUserTag = incomingFrag->tag_m;

    for (RecvDesc_t *
         WildRecvDesc =
         (RecvDesc_t *) privateQueues.PostedWildRecv.begin();
         WildRecvDesc !=
         (RecvDesc_t *) privateQueues.PostedWildRecv.end();
         WildRecvDesc = (RecvDesc_t *) WildRecvDesc->next) {
        // sanity check
        assert(WildRecvDesc->WhichQueue == POSTEDWILDIRECV);

        //
        // If we have a match...
        //
        int PostedIrecvTag = WildRecvDesc->posted_m.tag_m;
        if ((FragUserTag == PostedIrecvTag) ||
            (PostedIrecvTag == ULM_ANY_TAG)) {
            if (PostedIrecvTag == ULM_ANY_TAG && FragUserTag < 0) {
                continue;
            }
            // remove link - no need to lock, no one else can be changing
            //   this list
            WildRecvDesc->WhichQueue = ONNOLIST;
            privateQueues.PostedWildRecv.RemoveLinkNoLock(WildRecvDesc);

            return WildRecvDesc;
        }
    }

    return ReturnValue;
}

//!

RecvDesc_t *Communicator::checkSMPSpecificRecvListForMatch
    (SMPFragDesc_t * incomingFrag) {
    RecvDesc_t *ReturnValue = 0;
    int FragUserTag = incomingFrag->tag_m;
    //
    // Loop over the specific irecvs.
    //
    int SourceProcess = incomingFrag->srcProcID_m;
    for (RecvDesc_t *
         SpecificDesc =
         (RecvDesc_t *) privateQueues.
         PostedSpecificRecv[SourceProcess]->begin();
         SpecificDesc !=
         (RecvDesc_t *) privateQueues.
         PostedSpecificRecv[SourceProcess]->end();
         SpecificDesc = (RecvDesc_t *) SpecificDesc->next) {
        //
        // If we have a match...
        //
        int PostedIrecvTag = SpecificDesc->posted_m.tag_m;
        if ((FragUserTag == PostedIrecvTag)
            || (PostedIrecvTag == ULM_ANY_TAG)) {
            if (PostedIrecvTag == ULM_ANY_TAG && FragUserTag < 0) {
                continue;
            }
            // remove link - assumed upper layer handles locking
            privateQueues.PostedSpecificRecv[SourceProcess]->
                RemoveLinkNoLock(SpecificDesc);
	    SpecificDesc->WhichQueue=ONNOLIST;
            return SpecificDesc;
        }
    }

    return ReturnValue;
}

//!
//! There are both specific and wild irecvs available.  Get
//! copies of the sequence numbers.  'just_seq' gets just the
//! sequence part of the id, stripping off the notation about
//! which processor it is for.
//!

RecvDesc_t *Communicator::checkSMPSpecificAndWildRecvListForMatch
    (SMPFragDesc_t * incomingFrag, int *queueMatched) {
    RecvDesc_t *ReturnValue = 0;
    //
    // We know that when this is called, both specific and wild irecvs
    //  have been posted.
    //

    int SrcProc = incomingFrag->srcProcID_m;
    RecvDesc_t *SpecificDesc =
        (RecvDesc_t *) privateQueues.PostedSpecificRecv[SrcProc]->
        begin();
    RecvDesc_t *WildRecvDesc =
        (RecvDesc_t *) privateQueues.PostedWildRecv.begin();
    unsigned long SpecificSeq = SpecificDesc->irecvSeq_m;
    unsigned long WildSeq = WildRecvDesc->irecvSeq_m;
    int SendUserTag = incomingFrag->tag_m;

    //
    // Loop until we return.
    //
    while (true) {
        //
        // If the wild irecv is earlier than the specific one...
        //
        if (WildSeq < SpecificSeq) {
            //
            // If we have a match...
            //
            int WildIRecvTag = WildRecvDesc->posted_m.tag_m;
            if ((SendUserTag == WildIRecvTag)
                || (WildIRecvTag == ULM_ANY_TAG)) {
                if (!(WildIRecvTag == ULM_ANY_TAG && SendUserTag < 0)) {

                    // remove link from list
                    WildRecvDesc->WhichQueue = ONNOLIST;
                    privateQueues.PostedWildRecv.
                        RemoveLinkNoLock(WildRecvDesc);
                    *queueMatched = Communicator::WILD_RECV_QUEUE;

                    return WildRecvDesc;
                }
            }
            //
            // No match, go to the next.
            //
            WildRecvDesc = (RecvDesc_t *) WildRecvDesc->next;

            //
            // If that was the last wild one, just look at the
            // rest of the specific ones.
            //
            if (WildRecvDesc == privateQueues.PostedWildRecv.end()) {
                ReturnValue =
                    checkSMPSpecificRecvListForMatch(incomingFrag);
                *queueMatched = Communicator::SPECIFIC_RECV_QUEUE;

                return ReturnValue;
            }
            //
            // Get the sequence number for this new irecv, and
            // go back to the top of the loop.
            //
            WildSeq = WildRecvDesc->irecvSeq_m;
        }
        //
        // If the specific irecv is earlier than the wild one...
        //
        else {
            //
            // If we have a match...
            //
            int SpecificRecvTag = SpecificDesc->posted_m.tag_m;
            if ((SendUserTag == SpecificRecvTag)
                || (SpecificRecvTag == ULM_ANY_TAG)) {
                if (!(SpecificRecvTag == ULM_ANY_TAG && SendUserTag < 0)) {
                    // remove descriptor from list - upper layer handle locking
                    SpecificDesc->WhichQueue = ONNOLIST;
                    privateQueues.PostedSpecificRecv[SrcProc]->
                        RemoveLinkNoLock(SpecificDesc);
                    *queueMatched = Communicator::SPECIFIC_RECV_QUEUE;

                    return SpecificDesc;
                }
            }
            //
            // No match, go on to the next specific irecv.
            //
            SpecificDesc = (RecvDesc_t *) SpecificDesc->next;

            //
            // If that was the last specific irecv, process the
            // rest of the wild ones.
            //
            if (SpecificDesc ==
                privateQueues.PostedSpecificRecv[SrcProc]->end()) {

                ReturnValue = checkSMPWildRecvListForMatch(incomingFrag);
                *queueMatched = Communicator::WILD_RECV_QUEUE;

                return ReturnValue;
            }
            //
            // Get the sequence number for this irecv, and go
            // back to the top of the loop.
            //
            SpecificSeq = SpecificDesc->irecvSeq_m;
        }
    }
}

