/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*
 *  CTChannel.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#include <netdb.h>
#include <signal.h>
#include <strings.h>

#include "ctnetwork/CTChannel.h"
#include "internal/log.h"
#include "util/parsing.h"
#include "util/misc.h"

#define INVALID_SOCKET          -1
#define LISTENQ                 32
#define _CHNL_BLK_SIZE          10

#define tcpchannel              ((CTTCPChannel *)channel_m)


#define ulm_chk_err(x, errstr, ycmd) do { \
        int ret = x; \
        if ( ret < 0 ) { \
                _ulm_set_file_line(__FILE__, __LINE__) ; \
                _ulm_err ("[%d]:", getpid()) ; \
                _ulm_err errstr ; \
                _ulm_err("errno = %d\n", errno); \
                ycmd; \
        } \
} while (0)


/* factory methods for creating concrete channel instances. */



struct _chnl_cls_info
{
    const char              *_name;
    channel_create_fn       _construct_fn;
};



static struct _chnl_cls_info    **_chnl_factories = NULL;
static int      _tail = 0;
static int      _factory_sz = 0;
static struct sigaction         oldact;

static void _handle_sigpipe(int sig)
{
    // call any existing handler, then return
    if ( oldact.sa_handler )
        oldact.sa_handler(sig);
    return;
}


void _init_factories(void)
{
    _chnl_factories = (struct _chnl_cls_info **)malloc(sizeof(struct _chnl_cls_info *)*_CHNL_BLK_SIZE);
    if ( NULL == _chnl_factories )
    {
        fprintf(stderr, "Error: unable to initialize _chnl_factories array.\n");
    }
    bzero(_chnl_factories, sizeof(struct _chnl_cls_info *)*_CHNL_BLK_SIZE);
    _factory_sz = _CHNL_BLK_SIZE;

    /* add in default constructor for TCP channel */
    CTChannel::addChannelConstructor("CTTCPChannel",  &CTTCPChannel::createChannel);
}

void CTChannel::CTChannelInit()
{
    struct      sigaction               act;

    _init_factories();

    if ( 0 )
    {
        /* set to handle SIGPIPE */
        act.sa_handler = _handle_sigpipe;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_RESTART;
        if ( sigaction(SIGPIPE, &act, &oldact) < 0 )
        {
            // handle error
        }
    }
}



CTChannel *CTChannel::createChannel(const char *chnlClass, const char *connectionInfo)
{
    int                                     idx = 0;
    channel_create_fn       func;
    CTChannel                       *chnl = NULL;

    while ( idx < _tail )
    {
        if ( !strcmp(chnlClass, _chnl_factories[idx]->_name) )
        {
            func = _chnl_factories[idx]->_construct_fn;
            if ( func )
            {
                chnl = func(connectionInfo);
                break;
            }
        }
        idx++;
    }

    return chnl;
}


bool CTChannel::addChannelConstructor(const char *chnlClass, CTChannel *(*factoryFunc)(const char *))
{
    bool            ret = true;
    struct _chnl_cls_info   **ptr = NULL, *item = NULL;

    /* add to tail and increment tail. */
    if ( _tail == _factory_sz )
    {
        // resize array
        ptr = (struct _chnl_cls_info **)realloc(_chnl_factories,
                                                sizeof(struct _chnl_cls_info *)*(_tail + _CHNL_BLK_SIZE));
        if ( NULL == ptr )
        {
            fprintf(stderr, "Error: Unable to resize _chnl_factories.\n");
            ret = false;
        }
        else
        {
            _chnl_factories = ptr;
        }
    }

    if ( ret )
    {
        item = (struct _chnl_cls_info *)malloc(sizeof(struct _chnl_cls_info));
        if ( item )
        {
            item->_name = chnlClass;
            item->_construct_fn = factoryFunc;

            _chnl_factories[_tail++] = item;
        }
        else
        {
            fprintf(stderr, "Error: Unable to malloc for _chnl_cls_info item.\n");
            ret = false;
        }
    }

    return ret;
}




/*
 *
 *                      CTTCPChannel  Implementation
 *
 */


static CTChannelStatus _translate_err(int err)
{
    if ( 0 == err)
        return kCTChannelOK;
    else if ( (EPIPE == err)  || (ECONNRESET == err) )
        return kCTChannelConnLost;
    else if ( ETIMEDOUT == err )
        return kCTChannelTimedOut;
    else
        return kCTChannelError;
}


void CTTCPChannel::CTTCPChannelInit()
{
    bzero(&addr_m, sizeof(addr_m));
    sockfd_m = INVALID_SOCKET;
    blocking_m = true;
    timeoutSecs_m = 0;
}



CTChannel *CTTCPChannel::createChannel(const char *connectionInfo)
{
/*
  PRE:    connection info should look like:
  <host>;<port>
  e.g. "foo.somewhere.com;4444"
*/
    CTTCPChannel            *chnl = NULL;
    char                            **list;
    int                                     cnt;

    cnt = _ulm_parse_string(&list, connectionInfo, 1, ";");
    if ( 2 == cnt )
    {
        chnl = new CTTCPChannel(list[0], (unsigned short)atoi(list[1]));
    }
    free_double_carray(list, cnt);

    return (CTChannel *)chnl;
}



CTTCPChannel::CTTCPChannel(const char *host, unsigned short port)
{
    struct hostent *hostn;

    CTTCPChannelInit();

    /* translate host to IP addr */
    if ( (hostn = gethostbyname(host)) )
    {
        memcpy( &(addr_m.sin_addr), hostn->h_addr_list[0], sizeof(addr_m.sin_addr));
        addr_m.sin_port = htons(port);
        addr_m.sin_family = AF_INET;
    }

}

CTTCPChannel::CTTCPChannel(struct sockaddr_in *addr)
{

    CTTCPChannelInit();
    if ( NULL != addr )
    {
        memcpy(&addr_m, addr, sizeof(addr_m));
    }
}

CTTCPChannel::CTTCPChannel(int socketfd, struct sockaddr_in     *addr)
{
    CTTCPChannelInit();
    if ( INVALID_SOCKET != (sockfd_m = socketfd))
    {
        //  assume that we have a valid connection
        connected_m = true;
    }

    if ( NULL != addr )
    {
        memcpy(&addr_m, addr, sizeof(addr_m));
    }
}


CTTCPChannel::~CTTCPChannel()
{
    closeChannel();
}

/* checking status of channel */
bool CTTCPChannel::isReadable()
{
    fd_set                      readfds;
    struct timeval      timeout;
    int                         cnt;

    // Socket must be created and connected

    if ( INVALID_SOCKET == sockfd_m )
    {
        // raise exception
        return false;
    }

    FD_ZERO(&readfds);
    FD_SET(sockfd_m, &readfds);

    // Create a timeout of zero (don't wait)

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Check socket for data

    cnt = select(sockfd_m + 1, &readfds, NULL, NULL, &timeout);

    if ( cnt < 0 )
    {
        // throw exception
    }

    return (1 == cnt);
}


bool CTTCPChannel::isWriteable()
{
    fd_set                      writefds;
    struct timeval      timeout;
    int                         cnt;

    // Socket must be created and connected

    if ( INVALID_SOCKET == sockfd_m )
    {
        // raise exception
        return false;
    }

    FD_ZERO(&writefds);
    FD_SET(sockfd_m, &writefds);

    // Create a timeout of zero (don't wait)

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Check socket for data

    cnt = select(sockfd_m + 1, NULL, &writefds, NULL, &timeout);

    if ( cnt < 0 )
    {
        // check error
        closeChannel();
    }

    return (1 == cnt);
}

/* send/recv methods. */
CTChannelStatus CTTCPChannel::sendData(const char *data, long int len, long int *sent)
{
    long int                        slen, bytesLeft;
    const char                  *ptr;
    CTChannelStatus         status = kCTChannelOK;

    *sent = 0;
    if ( false == isConnected() )
    {
        return kCTChannelClosed;
    }

    bytesLeft = len;
    ptr = data;
    do
    {
        slen = send(sockfd_m, ptr, bytesLeft, 0);
        if ( slen )
        {
            ptr += slen;
            bytesLeft -= slen;
        }
    } while ( ((slen < 0) && (EINTR == errno)) || ((slen > 0) && bytesLeft) );

    if ( slen < 0 )
    {
        // handle error
        status = _translate_err(errno);
        closeChannel();
    }
    else
        *sent = slen;

    return status;
}

CTChannelStatus CTTCPChannel::sendData(const ulm_iovec_t *iov, int iovcnt, long int *sent)
{
    long int                slen;
    CTChannelStatus         status = kCTChannelOK;

    *sent = 0;
    if ( false == isConnected() )
    {
        return kCTChannelClosed;
    }

    do
    {
        slen = writev(sockfd_m, (const struct iovec *)iov, iovcnt);
    } while ( (slen < 0) && (EINTR == errno) );

    if ( slen < 0 )
    {
        status = _translate_err(errno);
        fprintf(stderr, "Error in sending iovec. errno = %d\n", errno);
        // handle error
        closeChannel();
    }
    else
        *sent = slen;

    return status;
}


CTChannelStatus CTTCPChannel::receive(char *buffer, long int len, long int *lenrcvd)
{
    long int                slen, bytesLeft;
    char                        *ptr;
    CTChannelStatus status = kCTChannelOK;

    *lenrcvd = 0;
    if ( false == isConnected() )
    {
        return kCTChannelClosed;
    }

    bytesLeft = len;
    ptr = buffer;
    do
    {
        slen = recv(sockfd_m, ptr, bytesLeft, 0);
        if ( slen )
        {
            ptr += slen;
            bytesLeft -= slen;
        }
    } while ( ((slen < 0) && (EINTR == errno)) || ((slen > 0) && bytesLeft) );

    if ( slen < 0 )
    {
        status = _translate_err(errno);
        ulm_err(("Error in receiving. errno = %d\n", errno));
        // handle error
        closeChannel();
    }
    else if ( 0 == slen )
    {
        // Assume lost of connection
        closeChannel();
        status = kCTChannelClosed;
    }
    else
        *lenrcvd = len - bytesLeft;

    return status;
}



CTChannelStatus CTTCPChannel::receive(const ulm_iovec_t *iov, int iovcnt, long int *lenrcvd)
{
    long int                slen;
    CTChannelStatus status = kCTChannelOK;

    *lenrcvd = 0;
    if ( false == isConnected() )
    {
        return kCTChannelClosed;
    }
    do
    {
        slen = readv(sockfd_m, (const struct iovec *)iov, iovcnt);
    } while ( (slen < 0) && (EINTR == errno) );

    if ( slen < 0 )
    {
        // handle error
        status = _translate_err(errno);
        closeChannel();
    }
    else if ( 0 == slen )
    {
        // Assume lost of connection
        closeChannel();
        status = kCTChannelClosed;
    }
    else
        *lenrcvd = slen;

    return status;
}


CTChannelStatus CTTCPChannel::receiveAtMost(char *buffer, long int len, long int *lenrcvd)
{
    long int            slen;
    CTChannelStatus     status = kCTChannelOK;

    *lenrcvd = 0;
    if ( false == isConnected() )
    {
        return kCTChannelClosed;
    }

    do
    {
        slen = recv(sockfd_m, buffer, len, 0);
    } while ( (slen < 0) && (EINTR == errno) );

    if ( slen < 0 )
    {
        status = _translate_err(errno);
        ulm_err(("Error in receiving. errno = %d\n", errno));
        // handle error
        closeChannel();
    }
    else if ( 0 == slen )
    {
        // Assume lost of connection
        closeChannel();
        status = kCTChannelClosed;
    }
    else
        *lenrcvd = slen;

    return status;
}




/* msg-oriented methods. */

CTChannelStatus CTTCPChannel::getMessage(CTMessage **msg)
{
    char                    *buf;
    CTChannelStatus status;

    /*
      ASSERT: packed msg sent should look like:
      <msg length (long int)><packed msg>
    */
    *msg = NULL;
    status = getPackedMessage(&buf, NULL);
    if ( buf )
    {
        *msg = CTMessage::unpack(buf);
        free(buf);
    }

    return status;
}



CTChannelStatus CTTCPChannel::getPackedMessage(char **buffer, long int *msglen)
{
    char                            *msg = NULL;
    long int                        len, rlen;
    CTChannelStatus         status;

    /*
      ASSERT: packed msg sent should look like:
      <msg length (long int)><packed msg>
    */
    len = 0;
    status = receive((char *)&len, sizeof(len), &rlen);
    if ( kCTChannelOK == status )
    {
        //NOTE: this should be changed to be more robust, but
        // for now, use length to indicate error.
        if ( len <= 2 )
        {
            // we have an error
            //status = receive((char *)&err, len, &rlen);
            ulm_ferr(("Unable to receive packed msg.\n"));
            return kCTChannelError;
        }

        if ( (msg = (char *)malloc(sizeof(char)*len)) )
        {
            // Continue receiving until we get entire msg.
            status = receive(msg, len, &rlen);
            if ( rlen != len )
            {
                ulm_ferr(("Warning! only received %d bytes of expected %d bytes.\n",
                          len - rlen, len));
                free(msg);
                msg = NULL;
            }
        }
    }

    *buffer = msg;
    if ( msglen )
        *msglen = len;

    return status;
}

CTChannelStatus CTTCPChannel::sendError(int errNumber)
{
    long int                        len, rlen;
    char                            buf[12];
    CTChannelStatus         status;

    len = sizeof(errNumber);
    memcpy(buf, &len, sizeof(len));
    memcpy(buf+sizeof(len), &errNumber, len);

    status = sendData(buf, len+sizeof(len), &rlen);

    return status;
}


CTChannelStatus CTTCPChannel::sendMessage(CTMessage *msg)
{
    char                            *buf;
    long int                        len;
    CTChannelStatus         status;

    // pack up msg.
    if ( true == msg->pack(&buf, &len) )
    {
        status = sendPackedMessage(buf, len);
        free(buf);
    }
    else
        status = kCTChannelInvalidMsg;

    return status;
}



CTChannelStatus CTTCPChannel::sendPackedMessage(char *msg, long int msglen)
{
    long int                        rlen;
    CTChannelStatus         status;

    // send msg length
    status = sendData((char *)&msglen, sizeof(msglen), &rlen);

    // send msg data
    if ( kCTChannelOK == status )
        status = sendData(msg, msglen, &rlen);

    return status;
}



bool CTTCPChannel::openChannel(int timeoutSecs)
{
    bool            success = true, blk;

    if ( true == connected_m ) return true;

    ulm_chk_err((sockfd_m = socket(AF_INET, SOCK_STREAM, 0)),
                ("Unable to create socket."), return false );

    // attempt to connect
    if ( timeoutSecs > 0 )
    {
        // set timeout for connecting.  first, set nonblocking.
        blk = blocking_m;
        setIsBlocking(false);
        if (connect(sockfd_m, (struct sockaddr *)&addr_m, sizeof(struct sockaddr_in)) < 0)
        {
            if (errno != EWOULDBLOCK && errno != EINPROGRESS)
            {
                //fatal connect error.
                success = false;
            }
            else
            {
                fd_set          rset, wset;
                int                     err;
                struct timeval  to;

                FD_ZERO(&rset);
                FD_ZERO(&wset);
                FD_SET(sockfd_m, &rset);
                FD_SET(sockfd_m, &wset);
                to.tv_sec = timeoutSecs;
                to.tv_usec = 0;
                err = select(sockfd_m + 1, &rset, &wset, NULL, &to);
                if ( err < 0 )
                {
                    fprintf(stderr, "Timed out connection error in connecting.\n");
                }
                else if ( 0 == err )
                {
                    // timed out
                    success = false;
                }
            }

        }
        // restore blocking mode
        setIsBlocking(blk);
    }
    else
    {
        if (connect(sockfd_m, (struct sockaddr *)&addr_m, sizeof(struct sockaddr_in)) < 0)
        {
            success = false;
        }
    }

    if ( true == success )
    {
        connected_m = true;
    }
    else
        closeChannel();

    return success;
}




void CTTCPChannel::closeChannel()
{
    if ( INVALID_SOCKET != sockfd_m )
    {
        // raise exception
        close(sockfd_m);
        sockfd_m = INVALID_SOCKET;
    }
}




bool CTTCPChannel::setIsBlocking(bool tf)
{
    int     flags;

    if ( (INVALID_SOCKET == sockfd_m) )
        return false;

    flags = fcntl(sockfd_m, F_GETFL, 0);
    if ( -1 == flags )
        return false;

    if ( false == tf )
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;

    if ( fcntl(sockfd_m, F_SETFL, flags) == 0 )
    {
        blocking_m = tf;
    }

    return true;
}


void CTTCPChannel::setTimeout(unsigned int secs)
{
#ifdef FIX_ME_ROB
    unsigned int            tout;
#endif

    CTChannel::setTimeout(secs);

#ifdef FIX_ME_ROB
    if ( tout )
    {
    }
#endif
}



/*
 *
 *                      CTTCPSvrChannel  Implementation
 *
 */


CTTCPSvrChannel::CTTCPSvrChannel(unsigned short portn)
{
    struct sockaddr_in      addr;

    bzero(&addr, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = portn;

    channel_m = new CTTCPChannel(&addr);
}


bool CTTCPSvrChannel::setupToAcceptConnections()
{
    const struct sockaddr_in        *addr;
    struct sockaddr_in                      socketInfo;
    int                                                     sockfd, flg;
#ifdef __linux__
    socklen_t                                       slen;
#else
    int                                                     slen;
#endif

    ulm_chk_err((sockfd = socket(AF_INET, SOCK_STREAM, 0)),
                ("Unable to create socket."), return false );

    tcpchannel->setSocketfd(sockfd);

    flg = 1;
    if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flg, sizeof(flg)) < 0 )
    {
        ulm_err(("Unable to set SO_REUSEADDR socket option!\n"));
    }

    addr = tcpchannel->socketAddress();
    ulm_chk_err((bind(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in))),
                ("Unable to bind socket."), tcpchannel->setSocketfd(INVALID_SOCKET); return false  );

    /* if the port is 0, then read back the actual port opened for socket. */
    if ( 0 == addr->sin_port )
    {
        slen = sizeof(struct sockaddr_in);
        if (getsockname(sockfd, (struct sockaddr *)&socketInfo, &slen) < 0) {
            ulm_err(("adminMessage::serverInitialize unable to get server socket information!\n"));
            return false;
        }
        tcpchannel->setPort(ntohs(socketInfo.sin_port));
    }

    ulm_chk_err((listen(sockfd, LISTENQ)),
                ("Unable to listen on socket."), tcpchannel->setSocketfd(INVALID_SOCKET); return false );

    return true;
}


CTChannelStatus CTTCPSvrChannel::acceptConnections(int timeoutSecs, CTChannel **client)
{
    bool                            done;
    int                                     sockfd, clientfd;
#ifdef __linux__
    socklen_t                       alen;
#else
    int                                     alen;
#endif
    struct sockaddr_in              clientaddr;
    CTChannelStatus         status;

    *client = NULL;
    status = kCTChannelOK;
    sockfd = tcpchannel->socketfd();
    ulm_chk_err(sockfd, ("Invalid socket number."), return kCTChannelError  );

    done = false;
    while ( false == done )
    {
        if ( timeoutSecs )
        {
            fd_set          rset;
            int                     err;
            struct timeval  to;
            bool                    blk;

            // set timeout for connecting.  first, set nonblocking.
            blk = tcpchannel->isBlocking();
            tcpchannel->setIsBlocking(false);

            FD_ZERO(&rset);
            FD_SET(sockfd, &rset);
            to.tv_sec = timeoutSecs;
            to.tv_usec = 0;
            err = select(sockfd + 1, &rset, NULL, NULL, &to);
            if ( err < 0 )
            {
                fprintf(stderr, "Error in accepting connections. errno = %d.\n", errno);
                status = kCTChannelError;
                done = true;
            }
            else if ( 0 == err )
            {
                // timed out
                status = kCTChannelTimedOut;
                done = true;
            }
            tcpchannel->setIsBlocking(blk);
        }

        if ( kCTChannelOK == status )
        {
            alen = sizeof(clientaddr);
            clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &alen);
            if ( clientfd < 0 )
            {
                // check for acceptable err.
                if ( EINTR != errno )
                    done = true;
            }
            else
            {
                if ( (*client = (CTChannel *) new CTTCPChannel(clientfd, &clientaddr)) )
                    ((CTTCPChannel *)*client)->setIsBlocking(true);
                done = true;
            }
        }
    }

    return status;
}
