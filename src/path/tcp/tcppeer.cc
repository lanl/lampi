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

#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "init/init.h"
#include "internal/malloc.h"
#include "path/common/BaseDesc.h"
#include "path/tcp/tcppath.h"
#include "path/tcp/tcpsend.h"


extern size_t unsentAcks;
extern size_t unsentAcksReturned;

TCPPeer::TCPPeer() :
    tcpPath(0),
    thisHost(0),
    thisProc(0),
    peerHost(0),
    peerProc(0),
    peerPort(0),
    tcpSocketsConnected(0)
{
    tcpPath = TCPPath::singleton();
    thisHost = tcpPath->getHost();
    thisProc = tcpPath->getProc();
}


TCPPeer::~TCPPeer()
{
}


void TCPPeer::setProc(long procID) 
{
    peerHost = global_proc_to_host(procID);
    peerProc = procID;
}


//
//  Called at startup to initialize the host addresses that
//  are available for this peer. If the peer has multiple NICs
//  we can create multiple sockets and stripe across them.
//  
//  Note that since this is done only at startup, the size
//  of the arrays are static, so we do not have to acquire
//  locks to access them.
//

void TCPPeer::setNumAddresses(int numAddrs)
{
    tcpSockets.size(numAddrs);
    for(int i=0; i<numAddrs; i++) {
         TCPSocket& tcpSocket = tcpSockets[i];
         ScopedLock lock(tcpSocket.lock);
         tcpSocket.close();
    }
}


void TCPPeer::setAddress(int index, const struct sockaddr_in& inaddr)
{
#if !defined(NDEBUG)
    if(index < 0 || index >= (int)tcpSockets.size()) {
        ulm_exit((-1, "TCPPeer::setAddress: invalid index %d\n", index));
    }
#endif
    TCPSocket& tcpSocket = tcpSockets[index];
    tcpSocket.addr = inaddr;
}


void TCPPeer::setPort(unsigned short port)
{
    peerPort = port;
    for(size_t i=0; i<tcpSockets.size(); i++) {
         TCPSocket& tcpSocket = tcpSockets[i];
         ScopedLock lock(tcpSocket.lock);
         tcpSocket.addr.sin_port = peerPort;
    }
}
                                                                                       

//
//  Called by ulm_finalize to check for data that needs to
//  be delivered.
//

bool TCPPeer::needsPush()
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sendFrag || tcpSocket.recvFrag)
            return true;
    }
    return false;
}


// 
//  Called by ulm_finalize to provide each path an opportunity to
//  free up any held resources.
//

void TCPPeer::finalize()
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        ScopedLock lock(tcpSocket.lock);
        if(!tcpSocket.isClosed()) {
            tcpPath->removeListener(tcpSocket.sd,this,Reactor::NotifyAll);
            tcpSocket.close();
        }
    }
}


//
//  A peer has connected and sent us a procID, which corresponds
//  to this peer instance. So, if we are not connected, accept the
//  connection request. If a connection is already in progress, 
//  accept the connection if the peers procID is lower than our own,
//  otherwise reject the connection request.
//

bool TCPPeer::acceptConnection(int sd)
{
    struct sockaddr_in addr;
#if defined(__linux__)
    socklen_t addrlen = sizeof(struct sockaddr_in);
#else
    int addrlen = sizeof(struct sockaddr_in);
#endif
    getpeername(sd, (struct sockaddr*)&addr, &addrlen);

    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.addr.sin_addr.s_addr == addr.sin_addr.s_addr) {
            ScopedLock lock(tcpSocket.lock);
            if (tcpSocket.sd < 0) {
                tcpSocket.sd = sd;
                if(sendConnectAck(tcpSocket)) { 
                    tcpSocket.retries = 0;
                    tcpSocket.state = S_CONNECTED;
                    setSocketOptions(tcpSocket.sd);
                    incrementSocketCount();
                    tcpSocket.flags |= (Reactor::NotifyRecv|Reactor::NotifyExcept);
                    tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
                }
                return true;
            } 
            else if (tcpSocket.isConnected() == false && peerProc < tcpPath->getProc()) {
                tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
                tcpSocket.close();
                tcpSocket.sd = sd;
                if(sendConnectAck(tcpSocket)) {
                    tcpSocket.retries = 0;
                    tcpSocket.state = S_CONNECTED;
                    setSocketOptions(tcpSocket.sd);
                    incrementSocketCount();
                    tcpSocket.flags |= (Reactor::NotifyRecv|Reactor::NotifyExcept);
                    tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
                }
                return true;
            }
        }
    }
    return false;
}


//
//  Do we know how to reach the specified host.
//

bool TCPPeer::canReach()
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.addr.sin_addr.s_addr != 0 &&
           tcpSocket.addr.sin_port != 0)
            return true;
    }
    return false;
}

//
//  If not connected to a peer, start the connection and return incomplete.
//  Otherwise, create send fragements for the message and start the send.
//

bool TCPPeer::send(SendDesc_t *message, bool *incomplete, int *errorCode) 
{
    // are we connected - if not must connect first
    size_t numSockets = tcpSockets.size();
    if(tcpSocketsConnected < numSockets) {
        if(startConnect(errorCode) == false) {
            *incomplete = true;
            return false;
        }
        if(tcpSocketsConnected == 0) {
            *incomplete = true;
            return true;
        }
    }

    // allocate frag descriptors
    TCPSendFrag *sendFrag;
    int num_to_alloc = message->numfrags - message->NumFragDescAllocated;
    for(int ndesc=0; ndesc < num_to_alloc; ndesc++) {
        int retval;
        sendFrag = TCPSendFrag::getElement(retval);
        if(retval != ULM_SUCCESS) {
            if(retval == ULM_ERR_OUT_OF_RESOURCE || retval == ULM_ERR_FATAL) {
                *errorCode = retval;
                *incomplete = true;
                return false;
            }
            continue;
        }

        retval = sendFrag->init(this, message);
        if(retval != ULM_SUCCESS) {
            if(retval == ULM_ERR_OUT_OF_RESOURCE || retval == ULM_ERR_FATAL) {
                *errorCode = retval;
                *incomplete = true;
                return false;
            }
            continue;
        }
        message->FragsToSend.AppendNoLock(sendFrag);
        message->NumFragDescAllocated++;
    }

    sendStart(message);

    // have all fragments been sent
    *incomplete = ((unsigned)message->NumSent < message->numfrags);
    return true;
}

void TCPPeer::sendStart(SendDesc_t* message)
{
    // find unused sockets and start send for first fragments
    size_t numSockets = tcpSockets.size();
    for(size_t i=0; 
        i<numSockets && message->FragsToSend.size() && message->clearToSend_m; 
        i++) {

        TCPSocket& tcpSocket = tcpSockets[i];
        if(usethreads())
            tcpSocket.lock.lock();
 
        if(tcpSocket.isConnected() && tcpSocket.sendFrag == 0) {

            // dont send more than the first fragment until an ack is received
            // indicating that the matching receive has been posted
            if (message->numfrags > 1 && message->FragsToSend.size() == message->numfrags)
                message->clearToSend_m = false;

            // pull first fragment off queue
            TCPSendFrag *sendFrag = (TCPSendFrag*)message->FragsToSend.GetfirstElement();
            tcpSocket.sendFrag = sendFrag;
            sendFrag->WhichQueue = 0;
            if(usethreads())
                tcpSocket.lock.unlock();

            // start send, if it doesn't complete add to the select mask
            sendFrag->sendEventHandler(tcpSocket.sd);
            if(tcpSocket.sendFrag == sendFrag) {
                if(usethreads()) {
                    ScopedLock lock(tcpSocket.lock);
                    if(tcpSocket.sendFrag == sendFrag) { // double-checked lock
                        tcpSocket.flags |= Reactor::NotifySend;
                        tcpPath->insertListener(tcpSocket.sd, sendFrag, Reactor::NotifySend);
                    }
                } else { 
                    tcpSocket.flags |= Reactor::NotifySend;
                    tcpPath->insertListener(tcpSocket.sd, sendFrag, Reactor::NotifySend);
                }
            }

        } else if (usethreads()) 
            tcpSocket.lock.unlock();
    }
}


//
//  Create a socket and start a connection to the peer. Set the socket
//  to non-blocking, so the connect will return if the call would block.
//  To receive notification that the connection is complete, register
//  for send/write events on the select.
//

bool TCPPeer::startConnect(int *errorCode)
{
    size_t failed = 0;
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        ScopedLock lock(tcpSocket.lock);
        if (!tcpSocket.isClosed())
            continue;

        // has connection retry count been exceeded?
        if(tcpSocket.retries > TCPPath::MaxConnectRetries) {
            failed++;
            continue;
        }

        tcpSocket.sd = socket(AF_INET, SOCK_STREAM, 0);
        if (tcpSocket.sd < 0) {
            tcpSocket.retries++;
            *errorCode = ULM_ERR_TEMP_OUT_OF_RESOURCE;
            return false;
        }

        // set the socket to non-blocking
        int flags;
        if((flags = fcntl(tcpSocket.sd, F_GETFL, 0)) < 0) {
            ulm_err(("TCPPeer[%d,%d]::startConnect: fcntl(F_GETFL) failed with errno=%d\n", thisProc,peerProc,errno));
        } else {
            flags |= O_NONBLOCK;
            if(fcntl(tcpSocket.sd, F_SETFL, flags) < 0)
                ulm_err(("TCPPeer[%d,%d]::startConnect: fcntl(F_SETFL) failed with errno=%d\n", thisProc,peerProc,errno));
        }

        // start the connect - will likely fail with EINPROGRESS
        if(connect(tcpSocket.sd, (struct sockaddr*)&tcpSocket.addr, sizeof(struct sockaddr_in)) < 0) {
            // non-blocking so wait for completion
            if(errno == EINPROGRESS) {
                tcpSocket.state = S_CONNECTING;
                tcpSocket.flags |= (Reactor::NotifySend|Reactor::NotifyExcept);
                tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifySend|Reactor::NotifyExcept);
                continue;
            }
            tcpSocket.close();
            tcpSocket.retries++;
            continue;
        }
        if(sendConnectAck(tcpSocket)) {
            tcpSocket.state = S_CONNECT_ACK;
            tcpSocket.flags |= (Reactor::NotifySend|Reactor::NotifyExcept);
            tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
        }
    }

    // if we cannot connect any sockets, try an alternate path
    if(failed == tcpSockets.size()) {
        *errorCode = ULM_ERR_BAD_PATH;
        return false;
    }
    return true;
}


//
//  Check the asynchronous completion status of a connection request. When
//  the connection completes, register for recv/read events in the select
//  mask to receive the peers process ID.
//

void TCPPeer::completeConnect(TCPSocket& tcpSocket)
{
    int so_error = 0;
#if defined(__linux__)
    socklen_t length = sizeof(so_error);
#else
    int length = sizeof(so_error);
#endif

    // check connect completion status
    if(getsockopt(tcpSocket.sd, SOL_SOCKET, SO_ERROR, &so_error, &length) < 0) {
        ulm_err(("TCPPeer[%d,%d]::completeConnect(%d): getsockopt() failed with errno=%d\n", 
            thisProc, peerProc, tcpSocket.sd, errno));
        tcpSocket.close();
        tcpSocket.retries++;
        return;
    }
    if(so_error == EINPROGRESS) {
        tcpSocket.flags |= (Reactor::NotifySend|Reactor::NotifyExcept);
        tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifySend|Reactor::NotifyExcept);
        return;
    }
    if(so_error != 0) {
        ulm_err(("TCPPeer[%d,%d]::completeConnect(%d): connect() failed with errno=%d\n", 
            thisProc, peerProc, tcpSocket.sd, errno));
        tcpSocket.close();
        tcpSocket.retries++;
        return;
    }
    if(sendConnectAck(tcpSocket)) {
        tcpSocket.state = S_CONNECT_ACK;
        tcpSocket.flags |= (Reactor::NotifyRecv|Reactor::NotifyExcept);
        tcpPath->insertListener(tcpSocket.sd, this, Reactor::NotifyRecv|Reactor::NotifyExcept);
    }
}


bool TCPPeer::sendConnectAck(TCPSocket& tcpSocket)
{
    // send process rank to remote peer
    unsigned char* ptr = (unsigned char*)&thisProc;
    int cnt = 0;
    int len = sizeof(thisProc);
    while(cnt < len) {
        int retval = ::send(tcpSocket.sd, ptr+cnt, len-cnt, 0);
        if(retval < 0) {
            if(errno == EINTR)
                continue;
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                ulm_err(("TCPPeer[%d,%d]::sendConnectAck(%d): send() failed with errno=%d\n", 
                    thisProc, peerProc, tcpSocket.sd, errno));
                tcpSocket.close();
                return false;
            }
        }
        cnt += retval;
    }
    return true;
}


//
//  A file descriptor is available/ready for recv. Check the state of
//  the socket and take the appropriate action.
//

void TCPPeer::recvEventHandler(int sd)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sd == sd) {
            tcpSocket.lock.lock();
            if(tcpSocket.sd != sd) { // double-checked lock
                tcpSocket.lock.unlock();
                continue;
            }

            switch(tcpSocket.state) {
            case S_CONNECT_ACK:
                recvConnectAck(tcpSocket);
                tcpSocket.lock.unlock();
                break;
            case S_CONNECTED:
                {
                TCPRecvFrag* recvFrag = (TCPRecvFrag*)tcpSocket.recvFrag;
                if(recvFrag == 0) {
                    int retval;
                    recvFrag = TCPRecvFrag::getElement(retval);
                    if(retval != ULM_SUCCESS || recvFrag == 0)
                        return;
                    recvFrag->init(this);
                }
                tcpSocket.recvFrag = recvFrag;
                tcpSocket.lock.unlock();
                recvFrag->recvEventHandler(sd);
                break;
                }
            default:
                ulm_err(("TCPPeer[%d,%d]::recvEventHandler(%d): invalid socket state(%d).\n", 
                    thisProc, peerProc, sd, tcpSocket.state));
                tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
                tcpSocket.close(); 
                tcpSocket.lock.unlock();
                break;
            }
            break;
        }
    }
}

//
//  Receive the peers procID/rank and dispatch the connection
//  request to the appropriate TCPPeer instance.
//

void TCPPeer::recvConnectAck(TCPSocket& tcpSocket)
{
    // recv process rank from remote peer
    long peer = 0;
    unsigned char* ptr = (unsigned char*)&peer;
    int cnt = 0;
    int len = sizeof(peer);
    while(cnt < len) {
        int retval = ::recv(tcpSocket.sd, ptr+cnt, len-cnt, 0);

        // remote closed connection
        if(retval == 0) {
            tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
            tcpSocket.close();
            return;
        }

        // socket is non-blocking so handle errors on recv
        if(retval < 0) {
            if(errno == EINTR)
                continue;
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                ulm_err(("TCPPeer[%d,%d]::recvConnectAck(%d): recv() failed with errno=%d\n", 
                    thisProc, peerProc, tcpSocket.sd, errno));
                tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
                tcpSocket.close();
                return;
            }
        }
        cnt += retval;
    }

    // validate ack received from excpected peer
    if(peer != peerProc) {
        ulm_err(("TCPPeer[%d,%d]::recvConnectAck(sd): invalid peer: %d\n", 
            thisProc, peerProc, tcpSocket.sd, peer));
        tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
        tcpSocket.close();
        return;
    }
    tcpSocket.retries = 0;
    tcpSocket.state = S_CONNECTED;
    setSocketOptions(tcpSocket.sd);
    incrementSocketCount();
}

//
//  A file descriptor is ready for send. This should only be called
//  for file descriptors that are awaiting completion of an asynchronous
//  connect(). Otherwise, instances of TCPSendFrag receive 
//  notifications for data transfer.
//

void TCPPeer::sendEventHandler(int sd)
{
    tcpPath->removeListener(sd, this, Reactor::NotifyAll);
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sd == sd) {
            ScopedLock lock(tcpSocket.lock);
            if(tcpSocket.sd != sd)
                continue;

            if(tcpSocket.state == S_CONNECTING) {
                completeConnect(tcpSocket); 
            } else {
                ulm_err(("TCPPeer[%d,%d]::sendEventHandler(%d): invalid socket state(%d).\n", 
                    thisProc, peerProc, sd, tcpSocket.state));
                if (tcpSocket.isConnected())
                    decrementSocketCount();
                tcpSocket.close();
            }
            break;
        }
    }
}


//
//  An error has occurred on one of the open descriptors. Close the
//  connection and log the errror. 
//

void TCPPeer::exceptEventHandler(int sd)
{
    ulm_err(("TCPPeer[%d,%d]::exceptEventHandler(%d)\n", thisProc,peerProc,sd));
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sd == sd) {
            ScopedLock lock(tcpSocket.lock);
            if(tcpSocket.isConnected())
                decrementSocketCount();
            tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
            tcpSocket.close();
            break;
        }
    }
}



//
//  Called by TCPSendFrag when the fragment has been sent. Queue
//  another descriptor if pending or mark the message as complete.
//  Note that this is not called from TCPPeer::sendEventHandler,
//  rather from TCPSendFrag::sendEventHandler, so TCPSocket::lock
//  must be acquired.
//

void TCPPeer::sendComplete(TCPSendFrag* sendFrag)
{
    sendFrag->setSendDidComplete(true);
    
    // completed sending this frag, check for additional fragments to send
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sendFrag == sendFrag) {
            if(usethreads()) { 
                tcpSocket.lock.lock();
                if(tcpSocket.sendFrag != sendFrag) { // double-checked lock
                    tcpSocket.lock.unlock();
                    continue;
                }
            } else if(tcpSocket.sendFrag != sendFrag)
                continue;

            tcpSocket.sendFrag = 0;
            if(tcpSocket.flags & Reactor::NotifySend) {
                tcpSocket.flags &= ~Reactor::NotifySend;
                tcpPath->removeListener(tcpSocket.sd, sendFrag, Reactor::NotifySend);
            }

            // will send an ack for first fragment of a multi-fragment message,
            // or for the first fragment of a synchronous message
            SendDesc_t* message = sendFrag->getMessage();
            if(message->NumSent == 0 && 
               ((message->numfrags > 1 || message->sendType == ULM_SEND_SYNCHRONOUS)))
                ; // wait for ack
            else
                message->NumAcked++;  
            message->NumSent++; 
            sendFrag->ReturnDescToPool(getMemPoolIndex());

            // check to see if message is complete or need to send next fragment
            if ((unsigned)message->NumSent == message->numfrags) {
                if (message->messageDone == REQUEST_INCOMPLETE &&
                    message->sendType != ULM_SEND_SYNCHRONOUS)
                    message->messageDone = REQUEST_COMPLETE;
                if(usethreads()) tcpSocket.lock.unlock();
            } else if (message->clearToSend_m == false) {
                if(usethreads()) tcpSocket.lock.unlock();
            } else if (message->FragsToSend.size()) {
                // start next fragment
                TCPSendFrag *nextFrag = (TCPSendFrag*)message->FragsToSend.GetfirstElement();
                nextFrag->WhichQueue = 0;
                tcpSocket.sendFrag = nextFrag;
                if(usethreads()) tcpSocket.lock.unlock();

                nextFrag->sendEventHandler(tcpSocket.sd);
                if(tcpSocket.sendFrag == nextFrag) {
                    ScopedLock lock(tcpSocket.lock);
                    if(tcpSocket.sendFrag == nextFrag) {
                        tcpSocket.flags |= Reactor::NotifySend;
                        tcpPath->insertListener(tcpSocket.sd, nextFrag, Reactor::NotifySend);
                    }
                }
            } else if (usethreads())
                tcpSocket.lock.unlock();
            break;
        }
    }
}


//
//  Called by TCPRecvFrag to indicate that a receive has 
//  completed. Clears the flag indicating the socket is in use.
//  Called indirectly from TCPPeer::recvEventHandler which 
//  is already holding TCPSocket::lock.
//

void TCPPeer::recvComplete(TCPRecvFrag* recvFrag)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.recvFrag == recvFrag) {
            tcpSocket.recvFrag = 0;
        }
    }
}


//
//  Called by TCPRecvFrag to indicate that a receive has 
//  failed. Close the socket due to the error condition.
//  Called indirectly from TCPPeer::recvEventHandler which 
//  is already holding TCPSocket::lock.
//

void TCPPeer::recvFailed(TCPRecvFrag* recvFrag)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.recvFrag == recvFrag) {
            tcpPath->removeListener(tcpSocket.sd, this, Reactor::NotifyAll);
            tcpSocket.close();
            decrementSocketCount();
            return;
        }
    }
}


//
//  Start sending an ack for a received fragment if a
//  socket is available.
//

bool TCPPeer::send(TCPRecvFrag* recvFrag)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.isConnected() && tcpSocket.sendFrag == 0) {
            if(usethreads()) {
                ScopedLock lock(tcpSocket.lock);
                if(tcpSocket.sendFrag != 0)
                    continue;
                tcpSocket.sendFrag = recvFrag;
            } else {
                tcpSocket.sendFrag = recvFrag;
            }

            // start the send, if it doesn't complete add to the select mask
            recvFrag->sendEventHandler(tcpSocket.sd);
            if(tcpSocket.sendFrag == recvFrag) {
                ScopedLock lock(tcpSocket.lock);
                if(tcpSocket.sendFrag == recvFrag) {
                    tcpSocket.flags |= Reactor::NotifySend;
                    tcpPath->insertListener(tcpSocket.sd, recvFrag, Reactor::NotifySend);
                }
            }
            return true;
        }
    }
    return false;
}


//
//  Send of ack has completed so clear flag indicating
//  the socket is in use and clear it from select mask.
//  Caller is holding TCPSocket::lock
//

void TCPPeer::sendComplete(TCPRecvFrag* recvFrag)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sendFrag == recvFrag) {
            tcpSocket.sendFrag = 0;
            if(tcpSocket.flags & Reactor::NotifySend) {
                tcpSocket.flags &= ~Reactor::NotifySend;
                tcpPath->removeListener(tcpSocket.sd, recvFrag, Reactor::NotifySend);
            }
            return;
        }
    }
}


//
//  Send of ack has failed so clear flag indicating
//  the socket is in use and 
//  Caller is holding TCPSocket::lock
//

void TCPPeer::sendFailed(TCPRecvFrag* recvFrag)
{
    for(size_t i=0; i<tcpSockets.size(); i++) {
        TCPSocket& tcpSocket = tcpSockets[i];
        if(tcpSocket.sendFrag == recvFrag) {
            tcpPath->removeListener(tcpSocket.sd, recvFrag, Reactor::NotifySend);
            tcpSocket.close();
            decrementSocketCount();
            return;
        }
    }
}


void TCPPeer::setSocketOptions(int sd)
{
#if defined(__linux__)
    int optval = 1;
    socklen_t optlen = sizeof(optval);
#else
    int optval = 1;
    int optlen = sizeof(optval);
#endif

#if defined(TCP_NODELAY)
   optval = 1;
   if(setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &optval, optlen) < 0) {
       ulm_err(("TCPPeer[%d,%d] setsoskcopt(TCP_NODELAY) failed with errno=%d\n", thisProc,peerProc,errno));
   }
#endif
#if defined(TCP_NODELACK)
   optval = 1;
   if(setsockopt(sd, IPPROTO_TCP, TCP_NODELACK, &optval, optlen) < 0) {
       ulm_err(("TCPPeer[%d,%d] setsoskcopt(TCP_NODELAY) failed with errno=%d\n", thisProc,peerProc,errno));
   }
#endif
}

