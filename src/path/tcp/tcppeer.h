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

#ifndef _TCPPeer_
#define _TCPPeer_

#include <netinet/in.h>
#include "internal/state.h"
#include "util/DblLinkList.h"
#include "util/Lock.h"
#include "util/Reactor.h"
#include "util/Vector.h"
#include "util/ScopedLock.h"

class SendDesc_t;
class TCPPath;
class TCPSendFrag;
class TCPRecvFrag;

//
//  TCPPeer - an abstraction that represents one or more connections
//  to a peer process. An instance of TCPPeer is created for every
//  process in the group at startup. However, connections to the peer
//  are established dynamically on an as-needed basis:
//
//  Proc0                 Proc1
//  (1) connect()   --->  accept()
//  (2) send(procID) -->  recv(peerID) 
//                    
//   At this point Proc1 may choose to accept the new connection
//   or reject it. If Proc1 has not already attempted to connect
//   to Proc0, it will accept the connection by responding w/ its
//   own ProcID. If it has started a connection, but the peers procID
//   is lower than its own, it will use the new incoming connection
//   and close the connection it started.
//
//   (3) recv(peerID) <--  send(procID)
//
//   Otherwise, Proc1 will close the incoming connection and wait for
//   the connection it started to complete.
//
//   Once connected, dispatches sends/recvs to appropriate
//   sockets/descriptors.
//

class TCPPeer : Reactor::Listener {
public:
    TCPPeer();
    virtual ~TCPPeer();

    inline long getLocalHost() { return thisHost; }
    inline long getLocalProc() { return thisProc; }
    inline long getHost() { return peerHost; } 
    inline long getProc() { return peerProc; }
    inline unsigned short getPort() { return peerPort; }

    void setProc(long);
    void setNumAddresses(int count);
    void setAddress(int index, const struct sockaddr_in&);
    void setPort(unsigned short);

    bool acceptConnection(int sd);
    bool canReach();
    bool send(SendDesc_t* message, bool* incomplete, int *errorCode);
    void sendComplete(TCPSendFrag*);

    bool send(int sd, TCPRecvFrag*);
    void sendComplete(TCPRecvFrag*);
    void sendFailed(TCPRecvFrag*);
    void recvComplete(TCPRecvFrag*);
    void recvFailed(TCPRecvFrag*);

    bool needsPush();
    void finalize();

private:
    TCPPath *tcpPath;
    ProcessPrivateMemDblLinkList pendingSends;
    long thisHost;
    long thisProc;
    long peerHost;
    long peerProc;
    unsigned short peerPort;
    size_t tcpSocketsConnected;
    Locks lock;

    enum { S_CLOSED, S_CONNECTING, S_CONNECT_ACK, S_CONNECTED, S_FAILED };

    struct TCPSocket {
        TCPSocket() :
            sd(-1),
            state(S_CLOSED),
            flags(0),
            retries(0),
            sendFrag(0),
            recvFrag(0)
        {
        }
        struct sockaddr_in addr;
        int sd;
        int state;
        int flags;
        int retries;
        Reactor::Listener* sendFrag;
        Reactor::Listener* recvFrag;
        Locks lock;

        inline void close() {
            if(sd >= 0)
                ::close(sd);
            sd = -1;
            state = S_CLOSED;
            flags = 0;
            sendFrag = 0;  
            recvFrag = 0;
        }
        inline bool isClosed()    { return (state == S_CLOSED); }
        inline bool isConnected() { return (state == S_CONNECTED); }
    };
    Vector<TCPSocket> tcpSockets;

    bool allocateDescriptors(SendDesc_t* message, int *errorCode);
    virtual void sendEventHandler(int sd);
    virtual void recvEventHandler(int sd);
    virtual void exceptEventHandler(int sd);

    void completeConnect(TCPSocket&);
    void recvConnectAck(TCPSocket&);
    bool sendConnectAck(TCPSocket&);
    bool startConnect(int*);

    inline void incrementSocketCount() {
        if(usethreads()) {
            lock.lock();
            tcpSocketsConnected++;
            lock.unlock();
        } else
            tcpSocketsConnected++;
    }

    inline void decrementSocketCount() {
        if(usethreads()) {
            lock.lock();
            tcpSocketsConnected++;
            lock.unlock();
        } else
            tcpSocketsConnected++;
    }

    inline int getSocketCount() {
        if(usethreads()) {
            int count;
            lock.lock();
            count = tcpSocketsConnected;
            lock.unlock();
            return count;
        } else
            return tcpSocketsConnected;
    }

    void setSocketOptions(int);
};

#endif

