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

#include "path/tcp/tcppath.h"
#include "path/tcp/tcprecv.h"
#include "util/Vector.h"


FreeListPrivate_t<TCPRecvFrag> TCPRecvFrag::TCPRecvFrags;


//
//  One-time initialization at startup of the recv descriptor pool.
//

int TCPRecvFrag::initialize()
{
    int nFreeLists = local_nprocs();

    // fill in memory affinity index
    Vector<int> memAffinityPool;
    if(memAffinityPool.size(nFreeLists) == false)
        return ULM_ERROR;
    for(int i = 0 ; i < nFreeLists ; i++ ) 
        memAffinityPool[i] = i;

    int rc = TCPRecvFrags.Init(nFreeLists,
                               16,                   // pages per list
                               SMPPAGESIZE,          // pool chunk size
                               SMPPAGESIZE,          // page size
                               sizeof(TCPRecvFrag),  // element size
                               16,                   // min pages per list
                               -1,                   // max pages per list
                               1000,                 // max retries
                               " TCP recv frag descriptors ",
                               false,                // retry for resources
                               memAffinityPool.base(),
                               true,                 // enforce affinity
                               0,
                               true);                // abort when no resources
    return rc;
}


//
//  Initialization that occurs prior to a descriptor being used/reused.
//

void TCPRecvFrag::init(TCPPeer* tcpPeer)
{
    this->Init();
    this->tcpPeer = tcpPeer;
    this->thisProc = tcpPeer->getLocalProc();
    this->peerProc = tcpPeer->getProc();
    this->fragRequest = 0;
    this->fragHdrCnt = 0;
    this->fragAckCnt = 0;
    this->fragData = 0;
    this->fragCnt = 0;
    this->fragLen = 0;
    this->addr_m = 0;
    this->length_m = 0;
    this->fragAcked = false;
}


//
//  Offset of this fragment w/in entire message.
//

unsigned long TCPRecvFrag::dataOffset() 
{
    if (fragIndex_m == 0) 
        return 0;
    else
        return TCPPath::MaxEagerSendSize + ((fragIndex_m-1) * TCPPath::MaxFragmentSize);
}


//
//  Return a descriptor to pool, release any resources (e.g. temporary buffers).
//

void TCPRecvFrag::ReturnDescToPool(int localRank)
{
    tcpPeer = 0;
    if (fragData != 0) {
        ulm_free(fragData);
        fragData = 0;
    }
    WhichQueue = TCPRECVFRAGSFREELIST;
    TCPRecvFrags.returnElement(this, localRank);
}


//
//  Called by Reactor/TCPPeer::recvEventHandler to dispatch recv events to the
//  fragment. As the socket is non-blocking, continue to do partial reads until
//  the entire fragment is received.
//

void TCPRecvFrag::recvEventHandler(int sd)
{
    // check for a partial read of the header
    if(fragHdrCnt < sizeof(fragHdr))
        if(recvHeader(sd) == false)
            return;

    // continue to recv application data
    if (fragCnt < fragLen) 
        if(recvData(sd) == false)
            return;

    // recv and discard data that doesn't fit into app buffer
    if (fragCnt < length_m)
        if(recvDiscard(sd) == false)
            return;

    // pending recv is complete
    tcpPeer->recvComplete(this);

    // was recv already matched?
    if(fragRequest == 0) {

        // if the recv was not already matched, data has been buffered so simply
        // queue up the received fragment
        communicators[ctx_m]->handleReceivedFrag(this);

    } else {

        bool recvDone = false;
        if(fragData != 0) {
            // the data was non-contigous, need to copy into users buffer
            fragRequest->CopyToAppLock(this,&recvDone);
        } else {
            // otherwise, notify the request that the data has already 
            // been delivered to the application
            fragRequest->DeliveredToAppLock(fragLen, length_m-fragLen,&recvDone);
        }

        // do we need to send an ack?
        if(sendAck(sd))
            ReturnDescToPool(getMemPoolIndex());
        else  {
            WhichQueue = FRAGSTOACK;
            UnprocessedAcks.Append(this);
        }
        if(recvDone)
            fragRequest->messageDone = REQUEST_COMPLETE;
    }
}


//
//  Continue to do receives until entire header is read. Then setup 
//  the descriptor for receiving data. 
//

bool TCPRecvFrag::recvHeader(int sd)
{
    int cnt = -1; 
    while(cnt < 0) {
        cnt = recv(sd, (unsigned char*)&fragHdr + fragHdrCnt, sizeof(fragHdr) - fragHdrCnt, 0);
        if(cnt == 0) {
            tcpPeer->recvFailed(this);
            ReturnDescToPool(getMemPoolIndex());
            return false;
        }
        if(cnt < 0) {
            switch(errno) {
            case EINTR:
                continue;
            case EWOULDBLOCK:
                return false;
            default:
                ulm_err(("TCPRecvFrag[%d,%d]::recvHeader(%d): recv() failed with errno=%d\n", 
                    thisProc,peerProc,sd,errno));
                tcpPeer->recvFailed(this);
                ReturnDescToPool(getMemPoolIndex());
                return false;
            }
        }
    }

    // is the entire header available?
    fragHdrCnt += cnt;
    if(fragHdrCnt < sizeof(fragHdr))
        return false;

    // setup this descriptor
    fragLen = fragHdr.length;
    length_m = fragHdr.length;
    fragIndex_m = fragHdr.fragIndex_m;
    maxSegSize_m = TCPPath::MaxFragmentSize;
    msgLength_m = fragHdr.msg_length;
    tag_m = fragHdr.tag_m;
    ctx_m = EXTRACT_CTX((fragHdr.ctxAndMsgType));
    msgType_m = EXTRACT_MSGTYPE((fragHdr.ctxAndMsgType));
    isendSeq_m = fragHdr.isendSeq_m;
    seq_m = 0;
    seqOffset_m = dataOffset();

    Communicator *comm = communicators[ctx_m];
    srcProcID_m = comm->remoteGroup->mapGlobalProcIDToGroupProcID[fragHdr.src_proc];
    dstProcID_m = comm->localGroup->mapGlobalProcIDToGroupProcID[fragHdr.dst_proc];

    switch(fragHdr.type) {
    case TCP_MSGTYPE_MSG:
    {
        // attempt to match a posted receive
        fragRequest = (RecvDesc_t*)fragHdr.recv_desc.ptr;
        if (fragRequest == 0)
            fragRequest = (RecvDesc_t*)comm->matchReceivedFrag(this);
        if(fragRequest != 0) {
            ULMType_t *datatype = fragRequest->datatype;
            if(datatype == 0 || datatype->layout == CONTIGUOUS) {
                size_t offset = dataOffset();
                size_t appLength = fragRequest->posted_m.length_m - offset;
                if (appLength < fragLen)
                     fragLen = appLength;
                addr_m = ((unsigned char*)fragRequest->addr_m + offset);
            }
            sendAck(sd); // start an ack now as a match has already been made
        }

        // allocate a buffer for the receive
        if ((fragRequest == 0 || addr_m == 0) && fragLen > 0) {
            fragData = (unsigned char*)ulm_malloc(fragLen);
            if(fragData == 0) {
                ulm_err(("TCPRecvFrag[%d,%d]::recvEventHandler: ulm_malloc(%d) failed\n", 
                    thisProc,peerProc,fragLen));
                return false;
            }
            addr_m = fragData;
        }
        break;
    }
    case TCP_MSGTYPE_ACK:
    {
        SendDesc_t *message = (SendDesc_t*)fragHdr.send_desc.ptr;
        TCPSendFrag *frag;
        for(frag =  (TCPSendFrag *) message->FragsToSend.begin();
            frag != (TCPSendFrag *) message->FragsToSend.end();
            frag =  (TCPSendFrag *) frag->next)
        {
            frag->getHeader().recv_desc = fragHdr.recv_desc;
        }
        message->NumAcked++;
        message->clearToSend_m = true;
        tcpPeer->sendStart(message,sd);
        tcpPeer->recvComplete(this);
        ReturnDescToPool(getMemPoolIndex());
        return false;
    }
    default:
        ulm_err(("TCPRecvFrag[%d,%d]::recvHeader(%d): received invalid message type(%d)\n",
            thisProc,peerProc,sd,fragHdr.type));
        tcpPeer->recvFailed(this);
        ReturnDescToPool(getMemPoolIndex());
        return false;
    }
    return true;
}


//
//  Continue w/ non-blocking recv() calls until the entire
//  fragement is received.
//

bool TCPRecvFrag::recvData(int sd)
{
    int cnt = -1;
    while(cnt < 0) {
        cnt = recv(sd, (unsigned char*)addr_m+fragCnt, fragLen-fragCnt, 0);
        if(cnt == 0) {
            tcpPeer->recvFailed(this);
            ReturnDescToPool(getMemPoolIndex());
            return false;
        }
        if(cnt < 0) {
            switch(errno) {
            case EINTR:
                continue;
            case EWOULDBLOCK:
                return false;
            default:
                ulm_err(("TCPRecvFrag[%d,%d]::recvEventHandler: recv() failed with errno=%d\n", 
                    thisProc,peerProc,sd));
                tcpPeer->recvFailed(this);
                ReturnDescToPool(getMemPoolIndex());
                return false;
            }
        }
    }
    fragCnt += cnt;
    return (fragCnt >= fragLen);
}


//
//  If the app posted a receive buffer smaller than the
//  fragment, receive and discard remaining bytes.
//

bool TCPRecvFrag::recvDiscard(int sd)
{
    int cnt = -1;
    while(cnt < 0) {
        Vector<unsigned char> rbuf;
        rbuf.size(length_m - fragCnt);
        cnt = recv(sd, rbuf.base(), length_m-fragCnt, 0);
        if(cnt == 0) {
            tcpPeer->recvFailed(this);
            ReturnDescToPool(getMemPoolIndex());
            return false;
        }
        if(cnt < 0) {
            switch(errno) {
            case EINTR:
                continue;
            case EWOULDBLOCK:
                return false;
            default:
                ulm_err(("TCPRecvFrag[%d,%d]::recvEventHandler: recv() failed with errno=%d\n", 
                    thisProc,peerProc,sd));
                tcpPeer->recvFailed(this);
                ReturnDescToPool(getMemPoolIndex());
                return false;
            }
        }
    }
    fragCnt += cnt;
    return (fragCnt >= length_m);
}


//
//  Send an ack on the first fragment of a multi-fragment message,
//  as soon as the receive has been matched. This allows the
//  data to be received directly into the users buffer.
//

bool TCPRecvFrag::sendAck(int sd)
{
    // send an ack for the first fragment of a multi-fragment message,
    // or if the message type is synchronous
    if ((msgType_m == MSGTYPE_PT2PT) &&
        (fragHdr.fragIndex_m != 0 || fragHdr.msg_length == fragHdr.length))
        return true;

    if (fragAcked == false) {
        // setup ack header 
        fragAck = fragHdr;
        fragAck.type = TCP_MSGTYPE_ACK;
        fragAck.src_proc = fragHdr.dst_proc;
        fragAck.dst_proc = fragHdr.src_proc;
        fragAck.length = 0;
        fragAck.recv_desc.ptr = fragRequest;

        // attempt to send the ack
        fragAcked = tcpPeer->send(sd, this);
    }
    return (fragAckCnt >= sizeof(fragAck));
}


//
//  Continue with non-blocking send() calls until the
//  entire ack header is delivered.
//

void TCPRecvFrag::sendEventHandler(int sd)
{
    int cnt=-1; 
    while(cnt < 0) {
        // FIXED
        cnt = send(sd, (unsigned char*)&fragAck + fragAckCnt, sizeof(fragAck)-fragAckCnt, 0);
        if(cnt < 0) { 
            switch(errno) {
            case EINTR:
                continue;
            case EWOULDBLOCK:
                return;
            default:
                {
                ulm_err(("TCPRecvFrag[%d,%d]::sendEventHandler(%d): send() failed with errno=%d\n",
                    thisProc,peerProc,sd,errno));
                tcpPeer->sendFailed(this);
                return;
                }
            }
        }
    }
    fragAckCnt += cnt;
    if(fragAckCnt >= sizeof(fragAck))
        tcpPeer->sendComplete(this);
}

