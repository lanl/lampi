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

#include <fcntl.h>
#include "path/tcp/tcppath.h"
#include "path/tcp/tcpsend.h"
#include "util/Vector.h"


FreeListPrivate_t<TCPSendFrag> TCPSendFrag::TCPSendFrags;


//
//  One-time initialization of the TCPSendFrag descriptor pool
//  that is called at startup.
//

int TCPSendFrag::initialize()
{
    int nFreeLists = local_nprocs();

    // fill in memory affinity index
    Vector<int> memAffinityPool;
    if(memAffinityPool.size(nFreeLists) == false)
        return ULM_ERROR;
    for(int i = 0 ; i < nFreeLists ; i++ ) 
        memAffinityPool[i] = i;

    return TCPSendFrags.Init(  nFreeLists,
                               16,                   // pages per list
                               SMPPAGESIZE,          // pool chunk size
                               SMPPAGESIZE,          // page size
                               sizeof(TCPSendFrag),  // element size
                               16,                   // min pages per list
                               -1,                   // max pages per list
                               1000,                 // max retries
                               " TCP send frag descriptors ",
                               false,                // retry for resources
                               memAffinityPool.base(),
                               true,                 // enforce affinity
                               0,
                               true);                // abort when no resources
}


//
//   Initialization of the TCPSendFrag instance on
//   a per message basis.
//

int TCPSendFrag::init(TCPPeer *tcpPeer, SendDesc_t *message) 
{
    BaseSendFragDesc_t::init();
    
    this->tcpPeer = tcpPeer;
    this->thisProc = tcpPeer->getLocalProc();
    this->peerProc = tcpPeer->getProc();

    this->WhichQueue = TCPFRAGSTOSEND;
    this->fragMsg = message;
    this->fragMsgIndex = message->NumFragDescAllocated;
    this->fragTimeStarted = -1;

    if (message->sendType == ULM_SEND_SYNCHRONOUS &&
        message->NumFragDescAllocated == 0)
        this->fragMsgType = MSGTYPE_PT2PT_SYNC;
    else
        this->fragMsgType = MSGTYPE_PT2PT;

    // determine offset and fragment length
    if(message->NumFragDescAllocated == 0) {
        this->fragMsgOffset = 0;
        if (message->posted_m.length_m > TCPPath::MaxEagerSendSize)
            this->fragLength = TCPPath::MaxEagerSendSize;
        else
            this->fragLength = message->posted_m.length_m;
    } else {
        this->fragMsgOffset = TCPPath::MaxEagerSendSize + 
            ((message->NumFragDescAllocated-1) * TCPPath::MaxFragmentSize);
        size_t leftToSend = message->posted_m.length_m - this->fragMsgOffset;
        if(leftToSend > TCPPath::MaxFragmentSize)
            this->fragLength = TCPPath::MaxFragmentSize;
        else
            this->fragLength = leftToSend;
    }

 
    // if the data is non-contigous determine the starting point in the type map
    bool nonContig = ((message->datatype != 0) && (message->datatype->layout != CONTIGUOUS));
    if(nonContig == false) {
        this->fragTmapIndex = -1;
    } else {
        ULMType_t *datatype = message->datatype;
        int dtype_cnt = this->fragMsgOffset / datatype->packed_size;
        size_t data_copied = dtype_cnt * datatype->packed_size;
        ssize_t data_remaining = (ssize_t)(this->fragMsgOffset - data_copied);
        this->fragTmapIndex = datatype->num_pairs - 1;
        for (int ti = 0; ti < datatype->num_pairs; ti++) {
            if (datatype->type_map[ti].seq_offset == data_remaining) {
                this->fragTmapIndex = ti;
                break;
            } else if (datatype->type_map[ti].seq_offset > data_remaining) {
                this->fragTmapIndex = ti - 1;
                break;
            }
        }
    }

    // setup the header
    Communicator *comm = communicators[message->ctx_m];
    tcp_msg_header& header  = this->fragHdr;
    header.type             = TCP_MSGTYPE_MSG;
    header.msg_length       = message->posted_m.length_m;
    header.send_desc.ptr    = message;
    header.recv_desc.ptr    = 0;
    header.src_proc         = comm->localGroup->mapGroupProcIDToGlobalProcID[comm->localGroup->ProcID];
    header.dst_proc         = comm->remoteGroup->mapGroupProcIDToGlobalProcID[message->posted_m.peer_m];
    header.tag_m            = message->posted_m.tag_m;
    header.ctxAndMsgType    = GENERATE_CTX_AND_MSGTYPE(message->ctx_m, this->fragMsgType);
    header.fragIndex_m      = this->fragMsgIndex;
    header.isendSeq_m       = message->isendSeq_m;

    this->fragVecs.size(2);
    this->fragVecPtr = this->fragVecs.base();
    this->fragVecs[0].iov_base = &header;
    this->fragVecs[0].iov_len  = sizeof(header);

    // setup the data
    if(this->fragLength == 0) {
        this->fragVecCnt = 1;
        header.length = 0;
    } else {
        this->fragVecCnt = 2;
        if(nonContig) {
            this->fragData = (unsigned char*)ulm_malloc(this->fragLength);
            if(this->fragData == 0)
                return ULM_ERR_OUT_OF_RESOURCE;
            packData(message);
        } else {
            // read data directly from application buffers
            this->fragData = 0;
            this->fragVecs[1].iov_base = ((caddr_t)message->addr_m + this->fragMsgOffset);
            this->fragVecs[1].iov_len = this->fragLength;
        }
        header.length = this->fragVecs[1].iov_len;
    } 
    return ULM_SUCCESS;
}


void TCPSendFrag::ReturnDescToPool(int localIndex)
{
    if(fragData != 0)
        ulm_free(fragData);
    tcpPeer = 0;
    fragMsg = 0;
    fragData = 0;
    WhichQueue = TCPFRAGFREELIST;
    TCPSendFrags.returnElement(this, localIndex);
}


//
//  Copied blatantly from UDP code. 
//
//  TODO: Investigate using an array of iovecs for non-contiguous
//  data rather than packing it.
//

void TCPSendFrag::packData(SendDesc_t* message)
{
    ULMType_t *dtype = message->datatype;
    ULMTypeMapElt_t *tmap = dtype->type_map;
    int tm_init = this->fragTmapIndex;
    int init_cnt = this->fragMsgOffset / dtype->packed_size;
    int tot_cnt = message->posted_m.length_m / dtype->packed_size;
    unsigned char *start_addr = ((unsigned char *) message->addr_m)
        + init_cnt * dtype->extent;

    // handle first typemap pair
    unsigned char* dest_addr = this->fragData;
    unsigned char* src_addr = start_addr
        + tmap[tm_init].offset
        - init_cnt * dtype->packed_size - tmap[tm_init].seq_offset + this->fragMsgOffset;
    size_t len_to_copy = tmap[tm_init].size
        + init_cnt * dtype->packed_size + tmap[tm_init].seq_offset - this->fragMsgOffset;
    len_to_copy = (len_to_copy > this->fragLength) ? this->fragLength : len_to_copy;

    bcopy(src_addr, dest_addr, len_to_copy);
    size_t len_copied = len_to_copy;

    tm_init++;
    for (int dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
        for (int ti = tm_init; ti < dtype->num_pairs; ti++) {
            src_addr = start_addr + tmap[ti].offset;
            dest_addr = (unsigned char *)this->fragData + len_copied;
            len_to_copy = (this->fragLength - len_copied >= tmap[ti].size) ?
                tmap[ti].size : this->fragLength - len_copied;
            if (len_to_copy == 0) {
                fragVecs[1].iov_base = this->fragData;
                fragVecs[1].iov_len = len_copied;
                return;
            }
            bcopy(src_addr, dest_addr, len_to_copy);
            len_copied += len_to_copy;
        }
        tm_init = 0;
        start_addr += dtype->extent;
    }
    fragVecs[1].iov_base = this->fragData;
    fragVecs[1].iov_len = len_copied;
}


//
//  Called by Reactor when socket is available for writing. 
//  Do non-blocking writes and wait until socket is available
//  to complete the sends.
//

void TCPSendFrag::sendEventHandler(int sd)
{
    int cnt=-1;
    while(cnt < 0) {
        cnt = ulm_writev(sd, fragVecPtr, fragVecCnt);
        if(cnt < 0) {
            switch(errno) {
            case EINTR:
                continue;
            case EWOULDBLOCK:
                return;
            default:
                {
                TCPPath* tcpPath = TCPPath::singleton();
                ulm_err(("TCPSendFrag[%d,%d]::sendEventHandler(%d): writev failed with errno=%d\n", 
                    thisProc,peerProc,sd,errno));
                tcpPath->removeListener(sd, this, Reactor::NotifyAll);
                return;
                }
            }
        }
    }

    int numVecs = fragVecCnt;
    for(int i=0; i<numVecs; i++) {
        if(cnt >= (int)fragVecPtr->iov_len) {
            cnt -= fragVecPtr->iov_len;
            fragVecPtr++;
            fragVecCnt--;
        } else {
            fragVecPtr->iov_base = ((unsigned char*)fragVecPtr->iov_base) + cnt;
            fragVecPtr->iov_len -= cnt;
            break;
        }
    }
    if(fragVecCnt == 0)
        tcpPeer->sendComplete(this);
}

