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

#ifndef _TCPRecvFrag_
#define _TCPRecvFrag_

#include "queue/globals.h"
#include "path/common/BaseDesc.h"
#include "path/tcp/tcphdr.h"


class TCPRecvFrag : public BaseRecvFragDesc_t, public Reactor::Listener {
public:
    TCPRecvFrag() {}
    TCPRecvFrag(int) {}

    static int initialize();
    inline static TCPRecvFrag* getElement(int& retval) { 
        return TCPRecvFrags.getElement(getMemPoolIndex(), retval); 
    }
    virtual void init(TCPPeer* tcpPeer);

    // BaseRecvFragDesc_t
    virtual void ReturnDescToPool(int localRank);
    virtual bool AckData(double timeNow = -1.0) { 
        return sendAck(-1); 
    }
    virtual unsigned int CopyFunction(void* fragAddr, void *appAddr, ssize_t length) {
        MEMCOPY_FUNC(fragAddr, appAddr, length); 
        return length; 
    }
    virtual unsigned long dataOffset();
    
    // Reactor::Listener
    virtual void recvEventHandler(int sd);
    virtual void sendEventHandler(int sd);

private:
    static FreeListPrivate_t <TCPRecvFrag> TCPRecvFrags;

    TCPPeer        *tcpPeer;
    long            thisProc;
    long            peerProc;

    RecvDesc_t     *fragRequest;
    tcp_msg_header  fragHdr;
    tcp_msg_header  fragAck;
    bool            fragAcked;
    size_t          fragHdrCnt;
    size_t          fragAckCnt;
    unsigned char*  fragData;
    size_t          fragCnt;
    size_t          fragLen;

    bool recvHeader(int sd);
    bool recvData(int sd);
    bool recvDiscard(int sd);
    bool sendAck(int sd);
};



#endif

