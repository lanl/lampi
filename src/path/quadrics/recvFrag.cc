/*
 * Copyright 2002.  The Regents of the University of California. This material
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

#include <elan3/elan3.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/state.h"
#include "client/daemon.h"
#include "queue/globals.h"	// for getMemPoolIndex()
#include "path/sharedmem/SMPSharedMemGlobals.h"	// for SMPSharedMemDevs and alloc..
#include "path/quadrics/recvFrag.h"
#include "path/quadrics/sendFrag.h"
#include "path/quadrics/path.h"

bool quadricsRecvFragDesc::AckData(double timeNow)
{
    ProcessPrivateMemDblLinkList *list;
    quadricsSendFragDesc *sfd;
    quadricsCtlHdr_t *hdr;
    quadricsDataAck_t *p;
    int returnValue, errorCode;

    /* only send an ACK if quadricsDoAck is false, if
     * the message is a synchronous send, and this is the
     * first message frag...
     */

    if (!quadricsDoAck) {
        /* free Elan-addressable memory now */
        if (length_m > CTLHDR_DATABYTES) {
            if ( usethreads() )
                quadricsState.quadricsLock.lock();            
            elan3_free(ctx, addr_m);
            if ( usethreads() )
                quadricsState.quadricsLock.unlock();            
        }
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

    // grab a send descriptor for the request ACK
    // and a malloc'ed quadricsCtlHdr_t from our FIFO,
    // if at all possible...

    hdr = (quadricsCtlHdr_t *)quadricsHdrs.get();
    if (!hdr) {
        return false;
    }
    if (usethreads())
        sfd = quadricsSendFragDescs.getElement(rail, returnValue);
    else
        sfd = quadricsSendFragDescs.getElementNoLock(rail, returnValue);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        free(hdr);
        path->cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), 
			&dummyCode);
        return false;
    }

    p = &(hdr->msgDataAck);
    p->thisFragSeq = seq_m;

    /* process the deliverd sequence number range */
    Communicator *pg = communicators[ctx_m];
    unsigned int glSourceProcess =  pg->remoteGroup->
	    mapGroupProcIDToGlobalProcID[srcProcID_m];
    returnValue=processRecvDataSeqs(p,glSourceProcess,reliabilityInfo);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        free(hdr);
        path->cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), 
			&dummyCode);
        return false;
    }

    // fill in other fields
    p->ctxAndMsgType = envelope.msgDataHdr.ctxAndMsgType;
    p->ptrToSendDesc = envelope.msgDataHdr.sendFragDescPtr;

    // use incoming ctx and rail!
    sfd->init( 0, ctx, MESSAGE_DATA_ACK, rail, envelope.msgDataHdr.senderID,
        -1, 0, 0, 0, 0, 0, false, hdr);

    list = &(quadricsQueue[rail].ctlMsgsToSend[MESSAGE_DATA_ACK]);
    if (usethreads()) {
        list->Lock.lock();
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlFlagsLock.lock();
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MESSAGE_DATA_ACK);
        quadricsQueue[rail].ctlFlagsLock.unlock();
        list->Lock.unlock();
    }
    else {
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MESSAGE_DATA_ACK);
    }

    path->sendCtlMsgs(rail, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);
    path->cleanCtlMsgs(rail, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);
    return true;
}



void quadricsRecvFragDesc::msgData(double timeNow)
{
    quadricsDataHdr_t *p = &(envelope.msgDataHdr);

    // fill in base info fields
    length_m = p->dataLength;
    msgLength_m = p->msgLength;
    if ( usethreads() )
        quadricsState.quadricsLock.lock();            
    addr_m = (length_m <= CTLHDR_DATABYTES) ? (void *)p->data :
        (void *)elan3_elan2main(ctx, (E3_Addr)p->dataElanAddr);
    if ( usethreads() )
        quadricsState.quadricsLock.unlock();            
    srcProcID_m = p->senderID;
    dstProcID_m = p->destID;
    tag_m = p->tag_m;
    ctx_m = EXTRACT_CTX(p->ctxAndMsgType);
    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    isendSeq_m = p->isendSeq_m;
    seq_m = p->frag_seq;
    seqOffset_m = dataOffset();
    fragIndex_m = seqOffset_m / quadricsBufSizes[PRIVATE_LARGE_BUFFERS];
    poolIndex_m = getMemPoolIndex();

    // check for valid communicator - if the communicator has already been released 
    // this is a duplicate fragment that can be dropped
    if(communicators[ctx_m] == NULL) {
        ReturnDescToPool(getMemPoolIndex());
        return;
    }

#if ENABLE_RELIABILITY
    isDuplicate_m = UNIQUE_FRAG;
#endif

    // make sure this was intended for this process...
    if (dstProcID_m != myproc()) {
        ulm_exit(("Quadrics data frag on rail %d from process %d misdirected to process "
                  "%d, intended destination %d\n",
                  rail, srcProcID_m, myproc(), dstProcID_m));
    }

    if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        // point to point message ...
        // call Communicator::handleReceivedFrag directly

        // remap process IDs from global to group ProcID
        dstProcID_m = communicators[ctx_m]->
            localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
        srcProcID_m = communicators[ctx_m]->
            remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

        // what do we do if the return value is not ULM_SUCCESS?
        communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this, timeNow);

    }
    else {
        ulm_exit(("Quadrics data envelope with invalid message "
                  "type %d\n", msgType_m));
    }
    return;
}

void quadricsRecvFragDesc::msgDataAck(double timeNow)
{
    quadricsDataAck_t *p = &(envelope.msgDataAck);
    quadricsSendFragDesc *sfd = (quadricsSendFragDesc *)p->ptrToSendDesc.ptr;
    volatile SendDesc_t *bsd = (volatile SendDesc_t *)sfd->parentSendDesc_m;

    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);

        // lock frag through send descriptor to prevent two
        // ACKs from processing simultaneously

        if (bsd) {
            ((SendDesc_t *)bsd)->Lock.lock();
            if (bsd != sfd->parentSendDesc_m) {
                ((SendDesc_t *)bsd)->Lock.unlock();
                ReturnDescToPool(getMemPoolIndex());
                return;
            }
        } else {
            ReturnDescToPool(getMemPoolIndex());
            return;
        }

#if ENABLE_RELIABILITY
        if (checkForDuplicateAndNonSpecificAck(sfd)) {
            ((SendDesc_t *)bsd)->Lock.unlock();
            ReturnDescToPool(getMemPoolIndex());
            return;
        }
#endif

        handlePt2PtMessageAck(timeNow, (SendDesc_t *)bsd, sfd);
        ((SendDesc_t *)bsd)->Lock.unlock();

    ReturnDescToPool(getMemPoolIndex());
    return;
}


void quadricsRecvFragDesc::memRel()
{
    quadricsMemRls_t *p = &(envelope.memRelease);

    // multiple threads are prevented from executing this by rcvLock...

    // is this a duplicate or misdirected?
    if ((p->destID != myproc()) || quadricsMemRlsSeqList[p->senderID].isRecorded(p->releaseSeq)) {
        ReturnDescToPool(getMemPoolIndex());
        return;
    }

    // NO, then free the memory
    switch (p->memType) {
    case PRIVATE_SMALL_BUFFERS:
    case PRIVATE_LARGE_BUFFERS:
        if ( usethreads() )
            quadricsState.quadricsLock.lock();            
        for (int i = 0; i < p->memBufCount; i++) {
            elan3_free(ctx, elan3_elan2main(ctx, (E3_Addr)(p->memBufPtrs.wordPtrs[i])));
        }
        if ( usethreads() )
            quadricsState.quadricsLock.unlock();            
        break;
    default:
        ulm_exit(("quadricsRecvFragDesc::memRel: bad memory "
                  "type %d!\n", p->memType));
        break;
    }

    // record serial number
    if (!quadricsMemRlsSeqList[p->senderID].record(p->releaseSeq)) {
        ulm_exit(("quadricsRecvFragDesc::memRel: unable to record "
                  "%lld in quadricsMemRlsSeqList[%d]\n",
                  p->releaseSeq, p->senderID));
    }

    ReturnDescToPool(getMemPoolIndex());
    return;
}

#define MEMREQ_PREFETCH_FACTOR ((int)3)

void quadricsRecvFragDesc::memReq(double timeNow)
{
    ProcessPrivateMemDblLinkList *list;
    quadricsMemReq_t *p = &(envelope.memRequest);
    quadricsSendFragDesc *sfd;
    quadricsCtlHdr_t *hdr;
    SMPSharedMem_logical_dev_t *dev = 0;
    void *addrs[MEMREQ_MAX_WORDPTRS];
    int naddrs = 0;
    int naddrsNeeded = 0;
    int returnValue = ULM_SUCCESS;
    int mtype = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    int bufType;
    int status = MEMREQ_SUCCESS;
    int errorCode;
    long long neededBytes = p->memNeededBytes;

    // grab a send descriptor for the request ACK
    // and a malloc'ed quadricsCtlHdr_t

    hdr = (quadricsCtlHdr_t *)ulm_malloc(sizeof(quadricsCtlHdr_t));
    if (!hdr) {
        ReturnDescToPool(getMemPoolIndex());
        return;
    }
    if (usethreads())
        sfd = quadricsSendFragDescs.getElement(rail, returnValue);
    else
        sfd = quadricsSendFragDescs.getElementNoLock(rail, returnValue);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        free(hdr);
        path->cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
        ReturnDescToPool(getMemPoolIndex());
        return;
    }

    // how many buffers, of what type?
    switch (mtype) {
    case MSGTYPE_PT2PT:
    case MSGTYPE_PT2PT_SYNC:
        if (neededBytes <= quadricsBufSizes[PRIVATE_SMALL_BUFFERS]) {
            bufType = PRIVATE_SMALL_BUFFERS;
            naddrsNeeded = 1;
        }
        else {
            bufType = PRIVATE_LARGE_BUFFERS;
            naddrsNeeded = (neededBytes + quadricsBufSizes[bufType] - 1)/
                quadricsBufSizes[bufType];
        }
        break;
    }

    /* prefetch destination buffers if we are not ACK'ing -- since we
     * can not reuse these buffers
     */
    if (!quadricsDoAck) {
        naddrsNeeded *= MEMREQ_PREFETCH_FACTOR;
    }

    if (naddrsNeeded > MEMREQ_MAX_WORDPTRS)
        naddrsNeeded = MEMREQ_MAX_WORDPTRS;

    // allocate the appropriate process-private
    // or shared memory (hopefully elan-addressable)
    for (int i = 0; i < naddrsNeeded; i++) {
        if ((mtype == MSGTYPE_PT2PT) || (mtype == MSGTYPE_PT2PT_SYNC)) {
            // elan-address process private memory
            if ( usethreads() )
                quadricsState.quadricsLock.lock();            
            addrs[i] = elan3_allocMain(ctx, E3_BLK_ALIGN,
                                       quadricsBufSizes[bufType]);
            if ( usethreads() )
                quadricsState.quadricsLock.unlock();            
            if (addrs[i])
                naddrs++;
            else {
                status = (naddrs == 0) ? MEMREQ_FAILURE_TMP : status;
                break;
            }
        }
        else {
            // shared memory pool -- hopefully elan-addressable...could add check...
            addrs[i] = allocPayloadBuffer(dev, quadricsBufSizes[bufType],
                                          &returnValue, getMemPoolIndex());;
            if (addrs[i] != (void *) -1L) {
                naddrs++;
            }
            else {
                if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
                    (returnValue == ULM_ERR_FATAL)) {
                    status = MEMREQ_FAILURE_FATAL;
                }
                else if (naddrs == 0) {
                    status = MEMREQ_FAILURE_TMP;
                }
                break;
            }
        }
    }

    // send the memory request results
    hdr->memRequestAck.requestStatus = status;
    hdr->memRequestAck.memBufCount = naddrs;
    hdr->memRequestAck.memBufSize = quadricsBufSizes[bufType] * naddrs;
    hdr->memRequestAck.memType = bufType;
    hdr->memRequestAck.memNeededSeqOffset = p->memNeededSeqOffset;
    hdr->memRequestAck.sendMessagePtr.ptr = p->sendMessagePtr.ptr;
    if ( usethreads() )
        quadricsState.quadricsLock.lock();            
    for (int i = 0; i < naddrs; i++) {
        // this translation works because all process/Elan combos
        // have the same virtual memory mappings
        hdr->memRequestAck.memBufPtrs.wordPtrs[i] =
            (ulm_uint32_t)elan3_main2elan(ctx, addrs[i]);
    }
    if ( usethreads() )
        quadricsState.quadricsLock.unlock();            

    // use incoming ctx and rail!
    sfd->init(0,
              ctx,
              MEMORY_REQUEST_ACK,
              rail,
              p->senderID,
              -1,
              0,
              0,
              0,
              0,
              0,
              false,
              hdr);
    // make sure that this message is not sent
    // over another rail...
    sfd->freeWhenDone = true;

    list = &(quadricsQueue[rail].ctlMsgsToSend[MEMORY_REQUEST_ACK]);
    if (usethreads()) {
        list->Lock.lock();
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlFlagsLock.lock();
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_REQUEST_ACK);
        quadricsQueue[rail].ctlFlagsLock.unlock();
        list->Lock.unlock();
    }
    else {
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_REQUEST_ACK);
    }

    path->sendCtlMsgs(rail, timeNow, MEMORY_REQUEST_ACK, MEMORY_REQUEST_ACK, &errorCode);
    path->cleanCtlMsgs(rail, timeNow, MEMORY_REQUEST_ACK, MEMORY_REQUEST_ACK, &errorCode);

    ReturnDescToPool(getMemPoolIndex());
    return;
}


void quadricsRecvFragDesc::memReqAck()
{
    quadricsMemReqAck_t *p = &(envelope.memRequestAck);
    void *addrs[MEMREQ_MAX_WORDPTRS];

    switch (p->requestStatus) {
    case MEMREQ_SUCCESS:
        // copy the 32-bit Elan addresses into a
        // void *array; a (void *) may be larger...
        for (int i = 0; i < p->memBufCount; i++) {
            addrs[i] = (void *)(p->memBufPtrs.wordPtrs[i]);
        }
        // push the addresses into quadricsPeerMemory
        // tracking structure
        quadricsPeerMemory[rail]->push((int)p->senderID, (int)p->memType, addrs, (int)p->memBufCount);
        break;
    case MEMREQ_FAILURE_FATAL:
        ulm_exit(("quadricsRecvFragDesc::memReqAck() fatal "
                  "memory request error from global"
                  " process %d\n", p->senderID));
        break;
    case MEMREQ_FAILURE_TMP:
    default:
        break;
    }

    ReturnDescToPool(getMemPoolIndex());
    return;
}
