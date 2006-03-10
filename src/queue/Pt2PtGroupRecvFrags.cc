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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "queue/Communicator.h"
#include "ulm/ulm.h"
#include "queue/globals.h"
#include "client/daemon.h"
#include "queue/globals.h"

//!
//! process received frag pulled "off the wire"
//!

int Communicator::handleReceivedFrag(BaseRecvFragDesc_t *DataHeader,
                                         double timeNow)
{
    RecvDesc_t *MatchedPostedRecvHeader;
    RequestDesc_t *request = 0;
    ULM_64BIT_INT fragSendSeqID = DataHeader->isendSeq_m;
    bool recvDone = false;

    //! get frag source (group ProcID ID)
    int fragSrc = DataHeader->srcProcID_m;

#if ENABLE_RELIABILITY
    // duplicate/dropped message frag support
    // only for those communication paths that overwrite
    // the frag_seq value from its default constructor
    // value of 0 -- valid sequence numbers are from
    // (1 .. (2^64 - 1))
    if (DataHeader->seq_m != 0) {
	bool sendthisack;
	bool recorded;
	unsigned int glfragSrc = remoteGroup->mapGroupProcIDToGlobalProcID[fragSrc];
	reliabilityInfo->dataSeqsLock[glfragSrc].lock();
	if (reliabilityInfo->receivedDataSeqs[glfragSrc].recordIfNotRecorded(DataHeader->seq_m, &recorded)) {

            ulm_warn(("Process rank %d (%s): Warning: "
                      "Received duplicate fragment [rank %d --> rank %d]: "
                      "ctx=%d, tag=%d, fraglength=%ld, msglength=%ld, fragseq=%ld, msgseq=%ld\n",
                      myproc(), mynodename(),
                      DataHeader->srcProcID_m,
                      DataHeader->dstProcID_m,
                      DataHeader->ctx_m,
                      DataHeader->tag_m,
                      (long) DataHeader->length_m,
                      (long) DataHeader->msgLength_m,
                      (long) DataHeader->seq_m,
                      (long) DataHeader->isendSeq_m));

	    // this is a duplicate from a retransmission...maybe a previous ACK was lost?
	    // unlock so we can lock while building the ACK...
	    if (reliabilityInfo->deliveredDataSeqs[glfragSrc].isRecorded(DataHeader->seq_m)) {
		// send another ACK for this specific frag...
		DataHeader->isDuplicate_m = DUPLICATE_DELIVERD;
		DataHeader->DataOK=ACKSTATUS_DATAGOOD;
		sendthisack = true;
	    }
	    else if (reliabilityInfo->receivedDataSeqs[glfragSrc].
		     largestInOrder() >= DataHeader->seq_m) {
		// send a non-specific ACK that should prevent this frag from being retransmitted...
		DataHeader->isDuplicate_m = DUPLICATE_RECEIVED;
		sendthisack = true;
	    } else {	// no acknowledgment is appropriate, just discard this frag and continue...
		sendthisack = false;
	    }

	    reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	    // do we send an ACK for this frag?
	    if (!sendthisack) {
		// return descriptor to pool
		DataHeader->ReturnDescToPool(getMemPoolIndex());
		return ULM_SUCCESS;
	    }
	    // yes we do, try to send it
	    if (DataHeader->AckData(timeNow)) {
		// return descriptor to pool
		DataHeader->
		    ReturnDescToPool(getMemPoolIndex());
	    } else {
		// move descriptor to list of unsent acks
		DataHeader->WhichQueue = FRAGSTOACK;
		UnprocessedAcks.Append(DataHeader);
	    }
	    return ULM_SUCCESS;
	}		// end duplicate frag processing
	// record the frag_seq number
	if (!recorded) {
	    reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	    ulm_exit(("Error: Unable to record frag sequence "
                      "number(2)\n"));
	}
	reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	// continue on...
    }
#endif

    //! get next expected sequence number - lock the sequence number to
    //!   make sure that if another thread is processing a frag
    //!   from the same message a match is posted only once.
    //!   This also keeps other threads from processing other frags
    //!   for this particluar send/recv process pair.
    if( usethreads() )
        next_expected_isendSeqsLock[fragSrc].lock();

    //! get sequence number of next message that can be processed
    ULM_64BIT_INT nextSeqIDToProcess =
	next_expected_isendSeqs[fragSrc];

    if (fragSendSeqID == nextSeqIDToProcess) {

	//!
	//! This is the sequence number we were expecting,
	//! so we can try matching it to irecvs.
	//!
	//! lock point-to-point receive to prevent any new receives
	//!   from being posted before this frag is processed
	//!
        if( usethreads() )
	    recvLock[fragSrc].lock();

	//! see if receive has already been posted

	MatchedPostedRecvHeader =
	    checkPostedRecvListForMatch(DataHeader);

	//! if match found, process data
	if (MatchedPostedRecvHeader) {

            /* process received data */
		request=(RequestDesc_t *)MatchedPostedRecvHeader;
		ProcessMatchedData(MatchedPostedRecvHeader,DataHeader, 
				timeNow, &recvDone);

		// Match just completed - scan privateQueues.OkToMatchRecvFrags list
		//  to see if any fragements have arrived between the
		//  irecv being posted (at which time no frags
    		//  for that message had arrived, and when this frag
    		//  is picked up. Need to search the list in any case
		//  just in case duplicate frags have arrived.
		if (!recvDone)
			SearchForFragsWithSpecifiedISendSeqNum(
					MatchedPostedRecvHeader, &recvDone,
					timeNow);
	} else {

	    //! if match not found, place on privateQueues.OkToMatchRecvFrags list

	    DataHeader->WhichQueue = UNMATCHEDFRAGS;
	    privateQueues.OkToMatchRecvFrags[fragSrc]->
		AppendNoLock(DataHeader);

	}

	//
	// We're now expecting the next sequence number.
	//
	++next_expected_isendSeqs[fragSrc];

	//!
	//! Handle old ahead of sequence messages from this proc
	//! that may have been freed up.
	//!
	if (privateQueues.AheadOfSeqRecvFrags[fragSrc]->size() > 0) {
	    matchFragsInAheadOfSequenceList(fragSrc, timeNow);
	}
	//! release locks
        if( usethreads() ){
            recvLock[fragSrc].unlock();
            next_expected_isendSeqsLock[fragSrc].unlock();
        }

	/* mark message as done, if it has completed - need to mark
	 *   it this late to avoid a race condition with another thread
	 *   waiting to complete a recv, completing, and try to free the
	 *   communicator before the current thread is done referencing
	 *   this communicator
	 */
	if( recvDone ){
        assert(MatchedPostedRecvHeader->messageDone != REQUEST_COMPLETE);
		MatchedPostedRecvHeader->messageDone = 
			REQUEST_COMPLETE;
	}
    } else if (fragSendSeqID < nextSeqIDToProcess) {

	//!
	//! lock receives to avoid a potential race condition
	//!   between a new recv being posted and "cleaning out"
	//!   the privateQueues.OkToMatchRecvFrags queue (never to be checked
	//!   again for posted message) and this frag being put
	//!   on the privateQueues.OkToMatchRecvFrags list, if no match is found.
        if( usethreads() )
	    recvLock[fragSrc].lock();

	//!
	//! This frag comes before the next expected, so it
	//! must be part of a fraged or corrupted message,
	//! if a match has already been made.   If a match
	//! has not been made, there is none to make.
	//! Look in the list of already matched irecvs.
	//!
	MatchedPostedRecvHeader = isThisMissingFrag(DataHeader);

	//! at this stage MatchedPostedRecvHeader is on a Matched
	//!   list, if match is found

	if (!MatchedPostedRecvHeader) {
	    //!
	    //! if match not found, place on privateQueues.OkToMatchRecvFrags list
	    //!
	    DataHeader->WhichQueue = UNMATCHEDFRAGS;
	    privateQueues.OkToMatchRecvFrags[fragSrc]->AppendNoLock(DataHeader);

	}
	//!
	//! other frags can now be processed safely w/o causing
	//!  these frags to be "lost" on the privateQueues.OkToMatchRecvFrags
	//!  queue.
        if( usethreads() ){
            next_expected_isendSeqsLock[fragSrc].unlock();

            //! receives can now be posted safely
            recvLock[fragSrc].unlock();
        }

	if (MatchedPostedRecvHeader) {
		//! copy data into specified buffer - DataHeader is also
		request=(RequestDesc_t *)MatchedPostedRecvHeader;
		ProcessMatchedData(MatchedPostedRecvHeader,
		 		DataHeader, timeNow, &recvDone);
		if( recvDone ){
            assert(MatchedPostedRecvHeader->messageDone != REQUEST_COMPLETE);
		    	MatchedPostedRecvHeader->messageDone = REQUEST_COMPLETE;
		}
	}
    } else {
	//
	// This message comes after the next expected, so it
	// is ahead of sequence.  Save it for later.
	//

    if (usethreads()) {
        recvLock[fragSrc].lock();
    }

	DataHeader->WhichQueue = AHEADOFSEQUENCEFRAGS;
	privateQueues.AheadOfSeqRecvFrags[fragSrc]->AppendNoLock(DataHeader);

	//! grant other threads access to frags
        if( usethreads() )
            recvLock[fragSrc].unlock();
            next_expected_isendSeqsLock[fragSrc].unlock();
    }

    return ULM_SUCCESS;
}

//!
//! process received frag pulled "off the wire"
//!

RecvDesc_t* Communicator::matchReceivedFrag(BaseRecvFragDesc_t *DataHeader,
                                         double timeNow)
{
    RecvDesc_t *MatchedPostedRecvHeader = 0;
    ULM_64BIT_INT fragSendSeqID = DataHeader->isendSeq_m;

    //! get frag source (group ProcID ID)
    int fragSrc = DataHeader->srcProcID_m;

#if ENABLE_RELIABILITY
    // duplicate/dropped message frag support
    // only for those communication paths that overwrite
    // the frag_seq value from its default constructor
    // value of 0 -- valid sequence numbers are from
    // (1 .. (2^64 - 1))
    if (DataHeader->seq_m != 0) {
	bool sendthisack;
	bool recorded;
	unsigned int glfragSrc = remoteGroup->mapGroupProcIDToGlobalProcID[fragSrc];
	reliabilityInfo->dataSeqsLock[glfragSrc].lock();
	if (reliabilityInfo->receivedDataSeqs[glfragSrc].recordIfNotRecorded(DataHeader->seq_m, &recorded)) {
	    // this is a duplicate from a retransmission...maybe a previous ACK was lost?
	    // unlock so we can lock while building the ACK...
	    if (reliabilityInfo->deliveredDataSeqs[glfragSrc].isRecorded(DataHeader->seq_m)) {
		// send another ACK for this specific frag...
		DataHeader->isDuplicate_m = true;
		sendthisack = true;
	    }
	    else if (reliabilityInfo->receivedDataSeqs[glfragSrc].
		     largestInOrder() >= DataHeader->seq_m) {
		// send a non-specific ACK that should prevent this frag from being retransmitted...
		DataHeader->isDuplicate_m = true;
		sendthisack = true;
	    } else {	// no acknowledgment is appropriate, just discard this frag and continue...
		sendthisack = false;
	    }

	    reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	    // do we send an ACK for this frag?
	    if (!sendthisack) {
		// return descriptor to pool
		DataHeader->
		    ReturnDescToPool(getMemPoolIndex());
		return 0;
	    }
	    // yes we do, try to send it
	    if (DataHeader->AckData(timeNow)) {
		// return descriptor to pool
		DataHeader->
		    ReturnDescToPool(getMemPoolIndex());
	    } else {
		// move descriptor to list of unsent acks
		DataHeader->WhichQueue = FRAGSTOACK;
		UnprocessedAcks.Append(DataHeader);
	    }
	    return 0;
	}		// end duplicate frag processing
	// record the frag_seq number
	if (!recorded) {
	    reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	    ulm_exit(("Error: Unable to record frag sequence "
                      "number(2)\n"));
	}
	reliabilityInfo->dataSeqsLock[glfragSrc].unlock();
	// continue on...
    }
#endif

    //! get next expected sequence number - lock the sequence number to
    //!   make sure that if another thread is processing a frag
    //!   from the same message a match is posted only once.
    //!   This also keeps other threads from processing other frags
    //!   for this particluar send/recv process pair.
    if( usethreads() )
        next_expected_isendSeqsLock[fragSrc].lock();

    //! get sequence number of next message that can be processed
    ULM_64BIT_INT nextSeqIDToProcess =
	next_expected_isendSeqs[fragSrc];

    if (fragSendSeqID == nextSeqIDToProcess) {

	//!
	//! This is the sequence number we were expecting,
	//! so we can try matching it to irecvs.
	//!
	//! lock point-to-point receive to prevent any new receives
	//!   from being posted before this frag is processed
	//!
        if( usethreads() )
	    recvLock[fragSrc].lock();

	//! see if receive has already been posted

	MatchedPostedRecvHeader =
	    checkPostedRecvListForMatch(DataHeader);

	//! if match found, process data
	if (MatchedPostedRecvHeader != 0) {

	    //
	    // We're now expecting the next sequence number.
	    //
	    ++next_expected_isendSeqs[fragSrc];
    
	    //!
	    //! Handle old ahead of sequence messages from this proc
	    //! that may have been freed up.
	    //!
	    if (privateQueues.AheadOfSeqRecvFrags[fragSrc]->size() > 0) {
	        matchFragsInAheadOfSequenceList(fragSrc, timeNow);
	    }
        }

	//! release locks
        if( usethreads() ){
            recvLock[fragSrc].unlock();
            next_expected_isendSeqsLock[fragSrc].unlock();
        }

    } else if (fragSendSeqID < nextSeqIDToProcess) {

	//!
	//! lock receives to avoid a potential race condition
	//!   between a new recv being posted and "cleaning out"
	//!   the privateQueues.OkToMatchRecvFrags queue (never to be checked
	//!   again for posted message) and this frag being put
	//!   on the privateQueues.OkToMatchRecvFrags list, if no match is found.
        if( usethreads() )
	    recvLock[fragSrc].lock();

	//!
	//! This frag comes before the next expected, so it
	//! must be part of a fraged or corrupted message,
	//! if a match has already been made.   If a match
	//! has not been made, there is none to make.
	//! Look in the list of already matched irecvs.
	//!
	MatchedPostedRecvHeader = isThisMissingFrag(DataHeader);

	//!
	//! other frags can now be processed safely w/o causing
	//!  these frags to be "lost" on the privateQueues.OkToMatchRecvFrags
	//!  queue.
        if( usethreads() ) {
            next_expected_isendSeqsLock[fragSrc].unlock();
    
            //! receives can now be posted safely
            recvLock[fragSrc].unlock();
        }

    } else {
	//
	// This message comes after the next expected, so it
	// is ahead of sequence.  Save it for later.
	//

    if (usethreads()) {
        recvLock[fragSrc].lock();
    }

	DataHeader->WhichQueue = AHEADOFSEQUENCEFRAGS;
	privateQueues.AheadOfSeqRecvFrags[fragSrc]->AppendNoLock(DataHeader);

	//! grant other threads access to frags
        if( usethreads() )
            recvLock[fragSrc].unlock();
            next_expected_isendSeqsLock[fragSrc].unlock();
    }
    return MatchedPostedRecvHeader;
}
