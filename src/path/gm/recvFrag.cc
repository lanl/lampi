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

#include "internal/log.h"
#include "internal/options.h"
#include "path/gm/recvFrag.h"
#include "util/dclock.h"

bool gmRecvFragDesc::AckData(double timeNow)
{
    gmHeaderDataAck *p;
    gmFragBuffer *buf;
    int rc;


    // If gmState.doAck is false, we still send and an ACK if the
    // message is a synchronous send, and this is the first fragment

    if (!gmState.doAck) {
        switch (msgType_m) {
        case MSGTYPE_PT2PT_SYNC:
		if (seqOffset_m != 0) {
			return true;
		}
    		break;
        default:
            return true;
            break;
        }
    }

    if (timeNow == -1.0) {
        timeNow = dclock();
    }

    // grab a fragment buffer for the ACK message

    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.lock();
    }
    buf = gmState.localDevList[dev_m].bufList.getElementNoLock(0, rc);
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.unlock();
    }
    if (rc != ULM_SUCCESS) {
        return false;
    }

    p = (gmHeaderDataAck *) &(buf->header.dataAck);

    p->thisFragSeq = seq_m;

    if (0 && OPT_RELIABILITY) {  // bypass for now!!!

        Communicator *pg = communicators[ctx_m];
        unsigned int glSourceProcess = pg->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];

        if (isDuplicate_m) {
            p->ackStatus = ACKSTATUS_DATAGOOD;
        } else {
            p->ackStatus = DataOK ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
        }
        if ((msgType_m == MSGTYPE_PT2PT)
            || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
            // grab lock for sequence tracking lists
            if (usethreads())
                reliabilityInfo->dataSeqsLock[glSourceProcess].lock();

            // do we send a specific ACK...recordIfNotRecorded returns record status before attempting record
            bool recorded;
            bool send_specific_ack = reliabilityInfo->
                deliveredDataSeqs[glSourceProcess].recordIfNotRecorded(seq_m, &recorded);

            // record this frag as successfully delivered or not even received, as appropriate...
            if (!(isDuplicate_m)) {
                if (DataOK) {
                    if (!recorded) {
                        reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                        ulm_exit((-1,
                                  "gmRecvFragDesc::AckData(pt2pt) unable "
                                  "to record deliv'd sequence number\n"));
                    }
                } else {
                    if (!(reliabilityInfo->receivedDataSeqs[glSourceProcess].erase(seq_m))) {
                        reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                        ulm_exit((-1,
                                  "gmRecvFragDesc::AckData(pt2pt) unable "
                                  "to erase rcv'd sequence number\n"));
                    }
                    if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                        reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                        ulm_exit((-1,
                                  "gmRecvFragDesc::AckData(pt2pt) unable "
                                  "to erase deliv'd sequence number\n"));
                    }
                }
            } else if (!send_specific_ack) {
                // if the frag is a duplicate but has not been delivered to the user process,
                // then set the field to 0 so the other side doesn't interpret
                // these fields (it will only use the receivedFragSeq and deliveredFragSeq fields
                p->thisFragSeq = 0;
                p->ackStatus = ACKSTATUS_AGGINFO_ONLY;
                if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1,
                              "gmRecvFragDesc::AckData(pt2pt) unable to erase duplicate deliv'd sequence number\n"));
                }
            }

            p->receivedFragSeq = reliabilityInfo->
                receivedDataSeqs[glSourceProcess].largestInOrder();
            p->deliveredFragSeq = reliabilityInfo->
                deliveredDataSeqs[glSourceProcess].largestInOrder();

            // unlock sequence tracking lists
            if (usethreads())
                reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
        } else {
            // unknown communication type
            ulm_exit((-1,
                      "gmRecvFragDesc::AckData() unknown communication "
                      "type %d\n", msgType_m));
        }

    } else {                    // no reliability

        p->ackStatus = DataOK ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
        p->receivedFragSeq = 0;
        p->deliveredFragSeq = 0;

    }

    // fill in other fields of header

    p->type = MESSAGE_DATA_ACK;
    p->ctxAndMsgType = gmHeader_m->data.ctxAndMsgType;
    p->src_proc = myproc();
    p->dest_proc = gmHeader_m->data.senderID;
    p->ptrToSendDesc = gmHeader_m->data.sendFragDescPtr;
    p->thisFragSeq = 0;
    p->checksum = 0;


    // only send if we have an implicit send token
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.lock();
    }
    if (gmState.localDevList[dev_m].sendTokens) {
        gmState.localDevList[dev_m].sendTokens--;
    } else {
        gmState.localDevList[dev_m].bufList.returnElementNoLock((Links_t *) buf, 0);
        if (usethreads())
            gmState.localDevList[dev_m].Lock.unlock();
        return false;
    }
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.unlock();
    }

    // send the ACK

    gm_send_with_callback(gmState.localDevList[dev_m].gmPort,
		    p, gmState.log2Size, sizeof(gmHeader),
		    GM_LOW_PRIORITY,
	      	    gmState.localDevList[dev_m].remoteDevList[p->dest_proc].node_id,
	      	    gmState.localDevList[dev_m].remoteDevList[p->dest_proc].port_id,
	      	    ackCallback, (void *) buf);

    return true;
}


// GM call back function to free resources for data ACKs

void gmRecvFragDesc::ackCallback(struct gm_port *port,
                             void *context,
                             enum gm_status status)
{
    int dev;
    gmFragBuffer *buf = (gmFragBuffer *) context;
    
    // Find the device corresponding to this port
    for (dev = 0; dev < gmState.nDevsAllocated; dev++) {
        if ( gmState.localDevList[dev].gmPort == port) {
            break;
        }
    }

    if ( dev == gmState.nDevsAllocated )
    {
        ulm_err(("Error! Unable to match GM port.\n"));
        return;
    }
    
    // reclaim send token
    if (usethreads()) 
        gmState.localDevList[dev].Lock.lock();
    gmState.localDevList[dev].sendTokens++;
    if (usethreads()) 
        gmState.localDevList[dev].Lock.unlock();

    // return fragment buffer to free list
    if (usethreads()) {
        gmState.localDevList[dev].Lock.lock();
        gmState.localDevList[dev].bufList.returnElementNoLock((Links_t *) buf, 0);
        gmState.localDevList[dev].Lock.unlock();
    } else {
        gmState.localDevList[dev].bufList.returnElementNoLock((Links_t *) buf, 0);
    }
}


// Free send resources on reception of a data ACK

void gmRecvFragDesc::msgDataAck()
{
    gmSendFragDesc		*sfd;
    SendDesc_t 			*bsd;

    gmHeaderDataAck *p = &(gmHeader_m->dataAck);
    sfd = (gmSendFragDesc *)p->sendFragDescPtr.ptr;
    bsd = (SendDesc_t *) sfd->parentSendDesc_m;

    if ( NULL == bsd )
    {
        ulm_err(("Process %d: Inconsistency Error! Base send descriptor is NULL.\n"));
        return;
    }

    if (usethreads())
        bsd->Lock.lock();

    // revisit this when reliability is implemented!!!!
    (bsd->NumAcked)++;

    sfd->setDidReceiveAck(true);
    if ( sfd->sendDidComplete() )
        sfd->freeResources();

    if (usethreads())
        bsd->Lock.unlock();

    ReturnDescToPool(0);
}

// Initialize descriptor with data from fragment data header

void gmRecvFragDesc::msgData(double timeNow)
{
    gmHeaderData *p = &(gmHeader_m->data);

    DataOK = false;
    srcProcID_m = p->senderID;
    dstProcID_m = p->destID;
    tag_m = p->user_tag;
    ctx_m = EXTRACT_CTX(p->ctxAndMsgType);
    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    isendSeq_m = p->isendSeq_m;
    seq_m = p->frag_seq;
    seqOffset_m = p->dataSeqOffset;
    msgLength_m = p->msgLength;
    fragIndex_m = seqOffset_m / gmState.bufSize;
    if (0) { // debug
        ulm_warn(("%d received frag from %d (length %ld)\n"
                  "%d\tgmFragBuffer_m %p gmHeader_m %p addr_m %p sizeof(gmHeader) %ld dev_m %d Dest %d\n"
                  "%d\ttag_m %d ctx_m %d msgType_m %d isendSeq_m %ld seq_m %lld seqOffset_m %ld\n"
                  "%d\tmsgLength_m %ld fragIndex_m %d\n",
                  myproc(), srcProcID_m,length_m,
                  myproc(), gmFragBuffer_m, gmHeader_m, addr_m, (long)sizeof(gmHeader), dev_m, dstProcID_m,
                  myproc(), tag_m, ctx_m, msgType_m, isendSeq_m, seq_m, seqOffset_m,
                  myproc(), msgLength_m, fragIndex_m));
    }
    // remap process IDs from global to group ProcID
    dstProcID_m = communicators[ctx_m]->
        localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
    srcProcID_m = communicators[ctx_m]->
        remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

    communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this, timeNow);
}

