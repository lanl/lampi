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

#ifndef _TCPPath_
#define _TCPPath_

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "internal/state.h"
#include "ulm/ulm.h"
#include "queue/globals.h" /* for getMemPoolIndex() */
#include "path/common/path.h"
#include "path/tcp/tcpsend.h"
#include "path/tcp/tcprecv.h"
#include "path/tcp/tcppeer.h"
#include "util/Lock.h"
#include "util/HashTable.h"
#include "util/Reactor.h"
#include "util/Vector.h"


class TCPPath : public BasePath_t, public Reactor::Listener {
public:

    TCPPath(int pathHandle);
    virtual ~TCPPath();

    static const size_t DefaultFragmentSize;
    static const size_t DefaultEagerSendSize;
    static const int     DefaultConnectRetries;

    static size_t MaxFragmentSize;
    static size_t MaxEagerSendSize;
    static int    MaxConnectRetries;

    // init methods called once at startup
    static int initSetupParams(adminMessage*);
    static int initPreFork();
    int initPostFork();
    int initClient(int ifCount, struct sockaddr_in* inaddrs);
    int exchangeListenPorts(adminMessage*);

    // BasePath_t methods
    virtual bool receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL) 
        { tcpReactor.poll(); return true; }
    virtual bool canReach(int globalDestProcessID);
    virtual bool init(SendDesc_t *message);
    virtual bool send(SendDesc_t *message, bool *incomplete, int *errorCode);
    virtual bool needsPush();
    virtual void finalize();

    bool retransmitP(SendDesc_t *message) {return false;}
    bool resend(SendDesc_t *message, int *errorCode)
    {
        *errorCode = ULM_SUCCESS;
        return false;
    }

    int fragSendQueue() {
        // return the queue identifier for the fragsToSend queue, e.g. GMFRAGSTOSEND
        return TCPFRAGSTOSEND;
    }
    
    int toAckQueue() {
        // return the queue identifier for the fragsToAck queue, e.g. GMFRAGSTOACK
        return TCPFRAGSTOACK;
    }
    
    // accessor methods
    inline int  getHandle() { return tcpPathHandle; }
    inline long getHost() { return thisHost; }
    inline long getProc() { return thisProc; }
    TCPPeer*    getPeer(int globalProcID);

    inline void insertListener(int sd, Reactor::Listener* l, int flags) { tcpReactor.insertListener(sd,l,flags); }
    inline void removeListener(int sd, Reactor::Listener* l, int flags) { tcpReactor.removeListener(sd,l,flags); }

    static TCPPath* singleton();

private:
    static TCPPath* _singleton;
    static Locks _lock;

    long thisHost;
    long thisProc;
    int tcpPathHandle;
    int tcpListenSocket;
    unsigned short tcpListenPort;
    Reactor tcpReactor;
    Vector<TCPPeer> tcpPeers;

    virtual void recvEventHandler(int);
    virtual void exceptEventHandler(int);
    void acceptConnections();
};

#endif

