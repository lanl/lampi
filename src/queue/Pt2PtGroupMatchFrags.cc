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

#include "queue/Communicator.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"

//
// Match frag against already matched ireceives.
//
RecvDesc_t *Communicator::isThisMissingFrag(BaseRecvFragDesc_t *rec)
{
    RecvDesc_t *ReturnValue = 0;

    //
    // Loop over the already matched specific irecvs.
    //

    int SourceProcess = rec->srcProcID_m;

    // lock list for safe (threads) reading
    privateQueues.MatchedRecv[SourceProcess]->Lock.lock();

    for (RecvDesc_t *
             SpecificDesc =
             (RecvDesc_t *) privateQueues.MatchedRecv[SourceProcess]->
             begin();
         SpecificDesc !=
             (RecvDesc_t *) privateQueues.MatchedRecv[SourceProcess]->
             end(); SpecificDesc = (RecvDesc_t *) SpecificDesc->next) {
        //
        // The matching irecv has the sequence number and the source
        // proc recorded.
        //
        if ((rec->isendSeq_m == SpecificDesc->isendSeq_m) &&
            // rec->SourceProcess is the the real source, so always >=0
            (((int) (SourceProcess)) == SpecificDesc->reslts_m.peer_m)) {
            //
            // Match found
            //

            // unlock list
            privateQueues.MatchedRecv[SourceProcess]->Lock.unlock();

            return SpecificDesc;
        }
    }

    // unlock list
    privateQueues.MatchedRecv[SourceProcess]->Lock.unlock();

    return ReturnValue;
}

//
// for a given RecvDesc_t this routine searches the
//  list of privateQueues.OkToMatchRecvFrags for frags with the same "isend
//  sequence number".  The assumption is that a match has already
//  been made, and this is just a check of the list to see if any
//  more frags are present.
//   Note:  The built in assumption is that the only time this routine
//          will be called is after a match has been found while processing
//          an incoming fragment -- recvLock[SendingProc] must be held for
//          multi-threaded code!
//

void Communicator::SearchForFragsWithSpecifiedISendSeqNum
(RecvDesc_t *MatchedPostedRecvHeader, bool *recvDone, double timeNow)
{
    int SendingProc = MatchedPostedRecvHeader->reslts_m.peer_m;
    /* get sequence number */
    unsigned long SendingSequenceNumber =
        MatchedPostedRecvHeader->isendSeq_m;

    // loop over list of frags
    for (BaseRecvFragDesc_t *
             RecDesc =
             (BaseRecvFragDesc_t *) privateQueues.
             OkToMatchRecvFrags[SendingProc]->begin();
         RecDesc !=
             (BaseRecvFragDesc_t *) privateQueues.
             OkToMatchRecvFrags[SendingProc]->end();
         RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
        // sanity check
        assert(RecDesc->WhichQueue == UNMATCHEDFRAGS);

        if (RecDesc->isendSeq_m == SendingSequenceNumber) {

            // remove frag from privateQueues.OkToMatchRecvFrags list
            BaseRecvFragDesc_t *TmpDesc = RecDesc;
            RecDesc = (BaseRecvFragDesc_t *)
                privateQueues.OkToMatchRecvFrags[SendingProc]->
                RemoveLinkNoLock(RecDesc);
            /* process data */
            ProcessMatchedData(MatchedPostedRecvHeader, TmpDesc, timeNow,
                               recvDone);
            if (*recvDone)
                break;

        }                       // done processing  RecDesc
    }                           // done looping over privateQueues.OkToMatchRecvFrags

    return;
}

// for a given RecvDesc_t this routine searches the
//  list of privateQueues.OkToMatchRecvFrags for frags with the same tag
//  The assumption is that a match has already
//  been made, and this is just a check of the list to see if any
//  more frags are present.
void Communicator::SearchForFragsWithSpecifiedTag
(RecvDesc_t *MatchedPostedRecvHeader, bool *recvDone, double timeNow)
{
    int SendingProc = MatchedPostedRecvHeader->posted_m.peer_m;
    int tag = MatchedPostedRecvHeader->posted_m.tag_m;

    // lock list for thread safety
    if(usethreads()) {
        privateQueues.OkToMatchRecvFrags[SendingProc]->Lock.lock();
    }

    for (BaseRecvFragDesc_t *RecDesc =
             (BaseRecvFragDesc_t *) privateQueues.
             OkToMatchRecvFrags[SendingProc]->begin();
         RecDesc !=
             (BaseRecvFragDesc_t *) privateQueues.
             OkToMatchRecvFrags[SendingProc]->end();
         RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {

        // sanity check
        assert(RecDesc->WhichQueue == UNMATCHEDFRAGS);

        if (RecDesc->tag_m == tag) {
            // remove frag from privateQueues.OkToMatchRecvFrags list
            BaseRecvFragDesc_t *TmpDesc = RecDesc;
            RecDesc = (BaseRecvFragDesc_t *)
                privateQueues.OkToMatchRecvFrags[SendingProc]->
                RemoveLinkNoLock(RecDesc);
            // process data
            ProcessMatchedData(MatchedPostedRecvHeader, TmpDesc, timeNow,
                               recvDone);
            if (*recvDone) {
                break;
            }
        }                  
    }                      

    // unlock list
    if(usethreads()) {
        privateQueues.OkToMatchRecvFrags[SendingProc]->Lock.unlock();
    }

    return;
}


//
// Scan the list of frags that came in ahead of time to see if any
// can be processed at this time.  If they can, try and match the
// frags.  At this stage we expcet that the calling routine
// ensures that only one thread will be processing this list, and that
// thread safety of the lists is handled outside this routine.
//

int Communicator::matchFragsInAheadOfSequenceList(int proc,
                                                      double timeNow)
{
    int errorCode = ULM_SUCCESS;
    RequestDesc_t *request;
    //
    // Loop over all the out of sequence messages.
    //
    RecvDesc_t *MatchedPostedRecvHeader;
    BaseRecvFragDesc_t *TmpDesc;
    bool Found = true, recvDone;

    while ((privateQueues.AheadOfSeqRecvFrags[proc]->size() > 0) && Found) {

        Found = false;

        // get sequence number to look for
        unsigned long nextSeqIDToMatch = next_expected_isendSeqs[proc];

        for (BaseRecvFragDesc_t *
                 RecvDesc =
                 (BaseRecvFragDesc_t *) privateQueues.
                 AheadOfSeqRecvFrags[proc]->begin();
             RecvDesc !=
                 (BaseRecvFragDesc_t *) privateQueues.
                 AheadOfSeqRecvFrags[proc]->end();
             RecvDesc = (BaseRecvFragDesc_t *) (RecvDesc->next)) {
            //
            // If the message has the next expected seq from that proc...
            //
            if (RecvDesc->isendSeq_m == nextSeqIDToMatch) {
                Found = true;
                recvDone = false;
                //
                // check to see if this frag matches a posted message
                //

                MatchedPostedRecvHeader =
                    checkPostedRecvListForMatch(RecvDesc);

                //
                // Take it out of the ahead of sequence list.
                //
                TmpDesc = (BaseRecvFragDesc_t *)
                    privateQueues.AheadOfSeqRecvFrags[proc]->
                    RemoveLinkNoLock(RecvDesc);

                // process the frag
                if (MatchedPostedRecvHeader) {
			request=(RequestDesc_t *)MatchedPostedRecvHeader;
			ProcessMatchedData(MatchedPostedRecvHeader, RecvDesc,
			 		timeNow, &recvDone);
			if( recvDone ){
			    	request->messageDone = REQUEST_COMPLETE;
			}

                } else {

                    // if match not found, place on privateQueues.OkToMatchRecvFrags list
                    RecvDesc->WhichQueue = UNMATCHEDFRAGS;
                    privateQueues.OkToMatchRecvFrags[proc]->
                        AppendNoLock(RecvDesc);

                }

                // reset loop pointer
                RecvDesc = TmpDesc;

                if (recvDone) {
                    next_expected_isendSeqs[proc] += 1;
                    break;
                }
                //
                // check to see if any more frags of message are present
                //
                for (BaseRecvFragDesc_t *
                         RDesc = (BaseRecvFragDesc_t *) RecvDesc->next;
                     RDesc !=
                         (BaseRecvFragDesc_t *) privateQueues.
                         AheadOfSeqRecvFrags[proc]->end();
                     RDesc = (BaseRecvFragDesc_t *) RDesc->next) {
                    if (RDesc->isendSeq_m == nextSeqIDToMatch) {

                        // Take it out of the ahead of sequence list.
                        TmpDesc = (BaseRecvFragDesc_t *)
                            privateQueues.AheadOfSeqRecvFrags[proc]->
                            RemoveLinkNoLock(RDesc);

                        // process the frag
                        if (MatchedPostedRecvHeader) {
				request=(RequestDesc_t *)MatchedPostedRecvHeader;
				ProcessMatchedData(MatchedPostedRecvHeader,
				 		RDesc, timeNow, &recvDone);
				if( recvDone ){
				    	request->messageDone = REQUEST_COMPLETE;
				}
                        } else {

                            // if match not found, place on privateQueues.OkToMatchRecvFrags list
                            RDesc->WhichQueue = UNMATCHEDFRAGS;
                            privateQueues.OkToMatchRecvFrags[proc]->
                                AppendNoLock(RDesc);

                        }

                        // reset RDesc
                        RDesc = TmpDesc;

                        if (recvDone)
                            break;

                    }           // end if block
                }               // end RDesc loop

                //
                // We're now expecting the next sequence number.
                //
                next_expected_isendSeqs[proc] += 1;
                break;

            }                   // end of match code block
        }                       // end loop over privateQueues.AheadOfSeqRecvFrags frags

    }                           // end of while loop

    return errorCode;
}


//
// try and match frag to posted receives The calling routine
// garantees that no other thread will access the posted receive
// queues while this routine is being executed.
//
RecvDesc_t *Communicator::checkPostedRecvListForMatch
(BaseRecvFragDesc_t *RecvDesc)
{
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
    // since multiple incoming fragments (from different
    // sources could be processed simultaneously
    if (usethreads()) {
        privateQueues.PostedWildRecv.Lock.lock();
    }

    if (privateQueues.PostedSpecificRecv[SourceProcess]->size() == 0) {
        //
        // There are only wild irecvs, so specialize the algorithm.
        //
        ReturnValue = checkWildPostedRecvListForMatch(RecvDesc);
    } else if (privateQueues.PostedWildRecv.size() == 0) {
        //
        // There are only specific irecvs, so specialize the algorithm.
        //
        ReturnValue = checkSpecificPostedRecvListForMatch(RecvDesc);
    } else {
        //
        // There are some of each.
        //
        ReturnValue = checkSpecificAndWildPostedRecvListForMatch(RecvDesc);
    }

    if (usethreads()) {
        privateQueues.PostedWildRecv.Lock.unlock();
    } 

    return ReturnValue;
}


RecvDesc_t *Communicator::
checkWildPostedRecvListForMatch(BaseRecvFragDesc_t *rec)
{
    RecvDesc_t *ReturnValue = 0;
    //
    // Loop over the wild irecvs.
    //

    int FragUserTag = rec->tag_m;
    for (RecvDesc_t *
             WildDesc =
             (RecvDesc_t *) privateQueues.PostedWildRecv.begin();
         WildDesc !=
             (RecvDesc_t *) privateQueues.PostedWildRecv.end();
         WildDesc = (RecvDesc_t *) WildDesc->next) {
        // sanity check
        assert(WildDesc->WhichQueue == POSTEDWILDIRECV);

        //
        // If we have a match...
        //
        int PostedIrecvTag = WildDesc->posted_m.tag_m;
        if ((FragUserTag == PostedIrecvTag) ||
            (PostedIrecvTag == ULM_ANY_TAG)) {
            if (PostedIrecvTag == ULM_ANY_TAG && FragUserTag < 0) {
                continue;
            }
            //
            // fill in received data information
            //
            WildDesc->isendSeq_m = rec->isendSeq_m;
            WildDesc->reslts_m.length_m = rec->msgLength_m;
            WildDesc->reslts_m.peer_m = rec->srcProcID_m;
            WildDesc->reslts_m.tag_m = rec->tag_m;
            //
            // Mark that this is the matching irecv, and go
            // to process it.
            //
            ReturnValue = WildDesc;

            // remove this irecv from the postd wild ireceive list
            privateQueues.PostedWildRecv.RemoveLinkNoLock(WildDesc);

            //  add this descriptor to the matched ireceive list
            WildDesc->WhichQueue = MATCHEDIRECV;
            privateQueues.MatchedRecv[rec->srcProcID_m]->
                Append(WildDesc);

            // exit the loop
            break;
        }
    }
    //

    return ReturnValue;
}


RecvDesc_t *Communicator::checkSpecificPostedRecvListForMatch(BaseRecvFragDesc_t *rec)
{
    RecvDesc_t *ReturnValue = 0;
    int FragUserTag = rec->tag_m;
    //
    // Loop over the specific irecvs.
    //
    int SourceProcess = rec->srcProcID_m;
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
            //
            // fill in received data information
            //
            SpecificDesc->reslts_m.length_m = rec->msgLength_m;
            SpecificDesc->reslts_m.peer_m = rec->srcProcID_m;
            SpecificDesc->reslts_m.tag_m = rec->tag_m;
            SpecificDesc->isendSeq_m = rec->isendSeq_m;
            //
            // Mark that this is the matching irecv, and put this into the
            //   matched ireceive list
            //
            ReturnValue = SpecificDesc;
            // remove descriptor from posted specific ireceive list
            privateQueues.PostedSpecificRecv[SourceProcess]->
                RemoveLinkNoLock(SpecificDesc);
            // append to match ireceive list
            SpecificDesc->WhichQueue = MATCHEDIRECV;
            privateQueues.MatchedRecv[SourceProcess]->Append(SpecificDesc);
            return ReturnValue;
        }
    }

    return ReturnValue;
}

//
// There are both specific and wild irecvs available.  Get copies of
// the sequence numbers.  'just_seq' gets just the sequence part of
// the id, stripping off the notation about which processor it is for.
//
RecvDesc_t *Communicator::checkSpecificAndWildPostedRecvListForMatch
(BaseRecvFragDesc_t *rec)
{
    RecvDesc_t *ReturnValue = 0;
    //
    // We know that when this is called, both specific and wild irecvs
    //  have been posted.
    //

    int SrcProc = rec->srcProcID_m;
    RecvDesc_t *SpecificDesc =
        (RecvDesc_t *) privateQueues.PostedSpecificRecv[SrcProc]->
        begin();
    RecvDesc_t *WildDesc =
        (RecvDesc_t *) privateQueues.PostedWildRecv.begin();
    unsigned long SpecificSeq = SpecificDesc->irecvSeq_m;
    unsigned long WildSeq = WildDesc->irecvSeq_m;
    int SendUserTag = rec->tag_m;

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
            int WildIRecvTag = WildDesc->posted_m.tag_m;
            if ((SendUserTag == WildIRecvTag)
                || (WildIRecvTag == ULM_ANY_TAG)) {
                if (!(WildIRecvTag == ULM_ANY_TAG && SendUserTag < 0)) {
                    //
                    // fill in received data information
                    //
                    WildDesc->reslts_m.length_m = rec->msgLength_m;
                    WildDesc->isendSeq_m = rec->isendSeq_m;
                    WildDesc->reslts_m.peer_m = SrcProc;
                    WildDesc->reslts_m.tag_m = SendUserTag;

                    //
                    // Mark it and process it.
                    //
                    ReturnValue = WildDesc;
                    WildDesc->WhichQueue = MATCHEDIRECV;
                    // remove this irecv from the postd wild ireceive list
                    privateQueues.PostedWildRecv.
                        RemoveLinkNoLock(WildDesc);
                    //  add this descriptor to the matched ireceive list
                    privateQueues.MatchedRecv[SrcProc]->Append(WildDesc);

                    return ReturnValue;
                }
            }
            //
            // No match, go to the next.
            //
            WildDesc = (RecvDesc_t *) WildDesc->next;

            //
            // If that was the last wild one, just look at the
            // rest of the specific ones.
            //
            if (WildDesc ==
                (RecvDesc_t *) privateQueues.PostedWildRecv.end()) {
                
                ReturnValue = checkSpecificPostedRecvListForMatch(rec);
                return ReturnValue;
            }
            //
            // Get the sequence number for this new irecv, and
            // go back to the top of the loop.
            //
            WildSeq = WildDesc->irecvSeq_m;
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
                    //
                    // fill in received data information
                    //
                    SpecificDesc->reslts_m.length_m = rec->msgLength_m;
                    SpecificDesc->reslts_m.peer_m = SrcProc;
                    SpecificDesc->isendSeq_m = rec->isendSeq_m;

                    //
                    // Mark it and process it.
                    //
                    ReturnValue = SpecificDesc;
                    SpecificDesc->WhichQueue = MATCHEDIRECV;
                    // remove descriptor from posted specific ireceive list
                    privateQueues.PostedSpecificRecv[SrcProc]->
                        RemoveLinkNoLock(SpecificDesc);
                    // append to match ireceive list
                    privateQueues.MatchedRecv[SrcProc]->
                        Append(SpecificDesc);

                    return ReturnValue;
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
                (RecvDesc_t *) privateQueues.
                PostedSpecificRecv[SrcProc]->end()) {

                ReturnValue = checkWildPostedRecvListForMatch(rec);
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
