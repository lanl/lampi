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

#ifndef _TCPSendFrag_
#define _TCPSendFrag_

#include <sys/uio.h>
#include "queue/globals.h"
#include "path/common/BaseDesc.h"
#include "path/tcp/tcphdr.h"
#include "util/DblLinkList.h"
#include "util/Reactor.h"
#include "util/Vector.h"

class TCPPeer;


class TCPSendFrag : public BaseSendFragDesc_t, public Reactor::Listener {
public:
    TCPSendFrag() {}
    TCPSendFrag(int) {}

    static int initialize();  // one-time initialization
    static TCPSendFrag* getElement(int& retval) 
        { return TCPSendFrags.getElement(getMemPoolIndex(), retval); }

    // accessors
    inline SendDesc_t* getMessage() { return fragMsg; }

    // per-fragment initialization/cleanup
    virtual int init(TCPPeer* tcpPeer, SendDesc_t* message);
    virtual void ReturnDescToPool(int localRank);

    // called from select loop when socket can accept data
    virtual void sendEventHandler(int sd);

private:

    static FreeListPrivate_t<TCPSendFrag> TCPSendFrags;

    TCPPeer              *tcpPeer;
    long                  thisProc;
    long                  peerProc;

    SendDesc_t           *fragMsg;
    int                   fragMsgIndex;
    size_t                fragMsgOffset;
    int                   fragMsgType;
    size_t                fragLength;
    unsigned char*        fragData;
    double                fragTimeStarted;
    int                   fragTmapIndex;
    tcp_msg_header        fragHdr;
    Vector<ulm_iovec_t>   fragVecs;
    ulm_iovec_t*          fragVecPtr;
    int                   fragVecCnt;

    void packData(SendDesc_t*);
};

#endif
