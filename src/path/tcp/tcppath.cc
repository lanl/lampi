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

#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "init/init.h"
#include "internal/malloc.h"
#include "path/common/BaseDesc.h"
#include "path/common/pathContainer.h"
#include "path/tcp/tcppath.h"
#include "util/InetHash.h"
#include "util/ScopedLock.h"


TCPPath* TCPPath::_singleton = 0;
Locks    TCPPath::_lock;
size_t   TCPPath::MaxFragmentSize = 64*1024;
size_t   TCPPath::MaxEarlySendSize = 16*1024;


TCPPath::TCPPath(int handle) :
    thisHost(myhost()),
    thisProc(myproc()),
    tcpPathHandle(handle),
    tcpListenSocket(-1),
    tcpListenPort(0)
{
    pathType_m = TCPPATH;
}


TCPPath::~TCPPath()
{
}


TCPPath* TCPPath::singleton()
{
    if(_singleton == 0) {
        if(_singleton == 0) {
            int pathHandle;
            size_t tcpPathSize = sizeof(TCPPath);
            if(tcpPathSize > MAX_PATH_OBJECT_SIZE_IN_BYTES)
                return 0;
            _singleton = (TCPPath *) (pathContainer()->add(&pathHandle));
            if (_singleton != 0)
                new(_singleton) TCPPath(pathHandle);
        }
    }
    return _singleton;
}


//
//  Initialization of send/recv descriptors.
//

int TCPPath::initPreFork()
{
    // allocate send/recv socket descriptors
    int rc;
    if((rc = TCPSendFrag::init()) != ULM_SUCCESS)
        return rc;

    if((rc = TCPRecvFrag::init()) != ULM_SUCCESS) 
        return rc;
    return ULM_SUCCESS;
}

//
//  One time initialization for all processes. Create arrays of data
//  structures to represent each participating host (TCPHost) and
//  process (TCPPeer) that are indexed by host rank and global process
//  rank.
//

int TCPPath::initPostFork()
{
    // allocate per host/process data structures
    int numHosts = nhosts();
    if(tcpHosts.size(numHosts > 0 ? numHosts : 0) == false)
        return ULM_ERROR;

    // allow for multiple sockets connections per client
    int numProcs = nprocs();
    if(tcpPeers.size(numProcs > 0 ? numProcs : 0) == false)
        return ULM_ERROR;
    for(int i=0; i<numProcs; i++)
        tcpPeers[i].setProc(i);
    return ULM_SUCCESS;
}

//
//  Client specific initialization. Create a listen socket 
//  to accept incoming TCP connections.
// 

int TCPPath::initClients(int numAddrs, struct sockaddr_in *hosts)
{
    // initialize host addresses
    for(size_t i=0; i<tcpHosts.size(); i++) {
        TCPHost& tcpHost = tcpHosts[i];
        tcpHost.setNumAddrs(1);
        tcpHost.setHostAddr(0,hosts[i]);
    }

    for(size_t i=0; i<tcpPeers.size(); i++) {
        TCPPeer& tcpPeer = tcpPeers[i];
        tcpPeer.setHostAddrs();
    }

    // create a listen socket for incoming connections
    tcpListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(tcpListenSocket < 0) {
        ulm_err(("TCPPath::init: socket() failed with errno=%d", errno));
        return ULM_ERROR;
    }

    // bind to all addresses and dynamically assigned port
    struct sockaddr_in inaddr;
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = INADDR_ANY;
    inaddr.sin_port = 0;

    if(::bind(tcpListenSocket, (struct sockaddr*)&inaddr, sizeof(inaddr)) < 0) {
        ulm_err(("TCPPath::init:  bind() failed with errno=%d", errno));
        return ULM_ERROR;
    }

    // resolve system assignend port
#if defined(__linux__)
    socklen_t addrlen = sizeof(struct sockaddr_in);
#else
    int addrlen = sizeof(struct sockaddr_in);
#endif
    if(getsockname(tcpListenSocket, (struct sockaddr*)&inaddr, &addrlen) < 0) {
        ulm_err(("TCPPath::init: getsockname() failed with errno=%d", errno));
        return ULM_ERROR;
    }
    tcpListenPort = inaddr.sin_port;
    tcpPeers[thisProc].setPort(tcpListenPort);

    // setup listen backlog to maximum allowed by kernel
    if(listen(tcpListenSocket, SOMAXCONN) < 0) {
        ulm_err(("TCPPath::init: listen(SOMAXCONN) failed with errno=%d", errno));
        return ULM_ERROR;
    }

    // set socket up to be non-blocking, otherwise accept could block
    int flags;
    if((flags = fcntl(tcpListenSocket, F_GETFL, 0)) < 0) {
        ulm_err(("TCPPath::init: fcntl(F_GETFL) failed with errno=%d", errno));
        return ULM_ERROR;
    } else {
        flags |= O_NONBLOCK;
        if(fcntl(tcpListenSocket, F_SETFL, flags) < 0) {
            ulm_err(("TCPPath::init: fcntl(F_SETFL) failed with errno=%d", errno));
            return ULM_ERROR;
        }
    }

    // register for callbacks on receive (accept)
    tcpReactor.insertListener(tcpListenSocket, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
    return ULM_SUCCESS;
}

//
//  Called at startup to distribute TCP listen ports to all
//  participating processes.
//

int TCPPath::allgatherListenPort(adminMessage *admin)
{
    Vector<unsigned short> ports;
    int numPeers = tcpPeers.size();
    ports.size(numPeers);
    for(int i=0; i<numPeers; i++)
        ports[i] = tcpPeers[i].getPort();

    int rc = admin->allgather(&tcpListenPort, ports.base(), sizeof(unsigned short));
    if(rc != ULM_SUCCESS) {
        return rc;
    }

    for(int i=0; i<numPeers; i++)
        tcpPeers[i].setPort(ports[i]);
    return ULM_SUCCESS;    
}

//
//  Defined in BasePath_t - assume we can reach the destination if we know its address
//

bool TCPPath::canReach(int globalDestProcessID) 
{
    int destinationHostID = global_proc_to_host(globalDestProcessID);
    struct sockaddr_in sockAddr = getHostAddr(destinationHostID);
    return (sockAddr.sin_addr.s_addr != 0);
}


bool TCPPath::init(SendDesc_t *message) 
{
    message->numfrags = 1;
    message->clearToSend_m = true;
    if (message->posted_m.length_m > MaxEarlySendSize) {
        size_t remaining = message->posted_m.length_m - MaxEarlySendSize;
        message->numfrags += (remaining + MaxFragmentSize - 1) / MaxFragmentSize;
    }
    return true;
}

//
//  Defined in BasePath_t - dispatch send to appropriate TCPPeer
//

bool TCPPath::send(SendDesc_t *message, bool *incomplete, int *errorCode) 
{
    int globalDestProc = communicators[message->ctx_m]->remoteGroup->
        mapGroupProcIDToGlobalProcID[message->posted_m.peer_m];
    TCPPeer& tcpPeer = tcpPeers[globalDestProc];
    return tcpPeer.send(message, incomplete, errorCode);
}


//
//  Defined in BasePath_t - called from ulm_finalize to see if data
//  is still pending. Note that this is also currently called from 
//  ulm_make_progress(), and we really don't want to add this expense
//  there.
//

bool TCPPath::needsPush()
{
#if 0
    for(size_t i=0; i<tcpPeers.size(); i++) {
        TCPPeer& tcpPeer = tcpPeers[i];
        if(tcpPeer.needsPush())
            return true;
    }
#endif
    return false;
}

//
//  Defined in BasePath_t - called from ulm_finalize to clean up
//  any resources held by the path. Should also check for pending
//  data and send if required.
//

void TCPPath::finalize()
{
    for(size_t i=0; i<tcpPeers.size(); i++) {
        TCPPeer& tcpPeer = tcpPeers[i];
        tcpPeer.finalize();
    }
}

//
//  Accessor Methods
//

struct sockaddr_in TCPPath::getHostAddr()
{
    return getHostAddr(thisHost);
}

struct sockaddr_in TCPPath::getHostAddr(int globalHostID, int numAddr)
{
#if !defined(NDEBUG)
    if(globalHostID >= (int)tcpHosts.size()) {
        static struct sockaddr_in addr;
        ulm_err(("TCPPath::getHost: invalid host index."));
        return addr;
    }
#endif
    TCPHost& tcpHost = tcpHosts[globalHostID];
    return tcpHost.getHostAddr(numAddr);
}

TCPHost* TCPPath::getHost(int globalHostID)
{
#if !defined(NDEBUG)
    if(globalHostID >= (int)tcpHosts.size()) {
        ulm_err(("TCPPath::getHost: invalid host index."));
        return 0;
    }
#endif
    return &tcpHosts[globalHostID];
}

TCPPeer* TCPPath::getPeer(int globalProcID) 
{
#if !defined(NDEBUG)
    if(globalProcID >= (int)tcpPeers.size()) {
        ulm_err(("TCPPath::getPeer: invalid process index."));
        return 0;
    }
#endif
    return &tcpPeers[globalProcID];
}


//
//  Called by Reactor when sockets have data available for recv.
//  Registered sockets are the TCP listen socket and any sockets
//  pending completion of the connection handshake.
//

void TCPPath::recvEventHandler(int sd)
{
    // accept new connections on listen socket
    if(sd == tcpListenSocket) {
        acceptConnections();
        return;
    }
    tcpReactor.removeListener(sd, this, Reactor::NotifyAll);

    // receive the peers global process ID
    long peerProc = 0;
    int retval = ::recv(sd, &peerProc, sizeof(peerProc), 0);

    // peer could close this connection
    if(retval == 0) {
        close(sd);
        return;
    }

    if(retval != sizeof(peerProc)) {
        ulm_err(("TCPPath[%d]::recvEventHandler(%d): recv() failed, retval=%d  errno=%d\n", 
            thisProc, sd, retval, errno));
        close(sd);
        return;
    }

    // validate the ID
    long numPeers = tcpPeers.size();
    if(peerProc < 0 || peerProc >= numPeers) {
        ulm_err(("TCPPath[%d]::recvEventHandler(sd): invalid peer process ID: %d\n", 
            thisProc, sd, peerProc));
        close(sd);
        return;
    }

    // setup socket to be non-blocking
    int flags;
    if((flags = fcntl(sd, F_GETFL, 0)) < 0) {
        ulm_err(("TCPPath[%d]::init: fcntl(F_GETFL) failed with errno=%d", thisProc, errno));
        close(sd);
        return;
    } else {
        flags |= O_NONBLOCK;
        if(fcntl(sd, F_SETFL, flags) < 0) {
            ulm_err(("TCPPath[%d]::init: fcntl(F_SETFL) failed with errno=%d", thisProc, errno));
            close(sd);
            return;
        }
    }

    // verify the peer will accept the connection - may already be connected
    TCPPeer& tcpPeer = tcpPeers[peerProc];
    if(tcpPeer.acceptConnection(sd) == false) {
        close(sd);
    }
}

//
//  Called by TCPPath::recvEventHandler when the TCP listen
//  socket has pending connection requests. Accept incoming
//  requests and queue for completion of the connection handshake.
//  We wait for the peer to send a 4 byte global process ID(rank) 
//  to complete the connection.
//

void TCPPath::acceptConnections()
{
#if defined(__linux__)
    socklen_t addrlen = sizeof(struct sockaddr_in);
#else
    int addrlen = sizeof(struct sockaddr_in);
#endif
    while(true) {
        struct sockaddr_in addr;
        int sd = ::accept(tcpListenSocket, (struct sockaddr*)&addr, &addrlen);
        if(sd < 0) {
            if(errno == EINTR)
                continue;
            if(errno != EAGAIN || errno != EWOULDBLOCK)
                ulm_err(("TCPPath::recvEventHandler: accept() failed with errno %d.", errno));
            return;
        }

        
        // wait for receipt of data to complete the connect
        tcpReactor.insertListener(sd, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
    }
}


//
//  Called by Reactor on socket exception conditions. An error
//  on the listen socket is considered fatal, this should never
//  happen...
//

void TCPPath::exceptEventHandler(int sd)
{
    ulm_err(("TCPPath[%d]::exceptEventHandler(sd)\n", thisProc, sd));
    if(sd == tcpListenSocket) {
        ulm_exit((-1, "TCPPath::exceptEventHandler(%d): exception on listen socket.\n", thisProc, sd));
    }

    tcpReactor.removeListener(sd, this, Reactor::NotifyAll);
    close(sd);
}

