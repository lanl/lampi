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
 *  CTChannel.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#ifndef _ADMIN_CHANNEL_H_
#define _ADMIN_CHANNEL_H_

#include <errno.h>

extern int errno;

#include "internal/types.h"
#include "internal/MPIIncludes.h"
#include "ctnetwork/CTMessage.h"


class CTChannel;

typedef enum
{
    kCTChannelOK = 0,
    kCTChannelError,                    /* general channel error. */
    kCTChannelMalloc,                   /* unable to alloc mem. */
    kCTChannelClosed,                   /* channel is not open */
    kCTChannelConnLost,                 /* lost connection */
    kCTChannelInvalidMsg,               /* unable to pack/unpack msg or msg is NULL */
    kCTChannelTimedOut                 /* channel operation timed out. */
} CTChannelStatus;


typedef CTChannel               *(*channel_create_fn)(const char *);
/*
        param 1:        String that contains channel-specific connection info with the template
                                <Host>;<Connection info>
*/

class CTChannel
{
protected:
    bool                connected_m;
    unsigned int        timeoutSecs_m;
    unsigned int        tag_m;
                
public:
    /* factory methods for creating concrete channel instances. */
    static void CTChannelInit();
    /* this must be called prior to using the factory methods. */
                
    /* Class factory methods for dynamic channel creation. */
    static CTChannel *createChannel(const char *chnlClass, const char *connectionInfo);
    static bool addChannelConstructor(const char *chnlClass, channel_create_fn factoryFunc);
                
    CTChannel() : connected_m(false), timeoutSecs_m(0), tag_m(0) {}
                
    virtual bool openChannel(int timeoutSecs = 0) = 0;
    virtual void closeChannel() = 0;
                
    /* checking status of channel */
    virtual bool isReadable() =  0;
    virtual bool isWriteable() = 0;
                
    /* send/recv methods. */
    virtual CTChannelStatus sendData(const char *data, long int len, long int *sent) = 0;
    virtual CTChannelStatus sendData(const ulm_iovec_t *iov, int iovcnt, long int *sent) = 0;

    virtual CTChannelStatus receive(char *buffer, long int len, long int *lenrcvd) = 0;
    virtual CTChannelStatus receive(const ulm_iovec_t *iov, int iovcnt, long int *lenrcvd) = 0;
    virtual CTChannelStatus receiveAtMost(char *buffer, long int len, long int *lenrcvd) = 0;
                
    /* msg-oriented methods. */

    /* receiving data */
    virtual CTChannelStatus getMessage(CTMessage **msg) = 0;
    /*
      POST:     Caller is responsible for freeing msg object.
    */
                
    virtual CTChannelStatus getPackedMessage(char **buffer, long int *msglen) = 0;
    /*
      POST:     Caller is responsible for freeing buffer.
    */
                
    virtual CTChannelStatus sendPackedMessage(char *msg, long int msglen) = 0;

    /* sending data */
                
    virtual CTChannelStatus sendError(int errNumber) = 0;
    /*
      POST:     Sends an error status that the receiver's getMessage should be able to handle.
    */
                
    virtual CTChannelStatus sendMessage(CTMessage *msg) = 0;
                
    unsigned int timeout() {return timeoutSecs_m;}
    virtual bool setTimeout(unsigned int secs)
    {
        timeoutSecs_m = secs;
        return true;
    }
                
    unsigned int tag() {return tag_m;}
    void setTag(unsigned int tag) {tag_m = tag;}
                
    bool isConnected() {return connected_m;}
                
    virtual const char *className() = 0;
};


/*
        Defines the interface for a channel that is capable
        of accepting external connections.
*/
class CTChannelServer
{
protected:
        CTChannel    	*channel_m;

public:
        /*
        * Server related methods.
        */
    virtual bool setupToAcceptConnections() = 0;
    virtual CTChannelStatus acceptConnections(int timeoutSecs, CTChannel **client) = 0;

    virtual void setChannelsToCheckForRead(struct link_t *channels) = 0;
    virtual bool channelsReadyForReading(int to_secs, int to_usecs) = 0;
    virtual CTChannel **channelsReadable(struct link_t *channels, int *cnt) = 0;

    virtual CTChannel *channel() {return channel_m;}
};

/*
 *
 *                      TCP/IP Channel implementation
 *
 */
 
 

class CTTCPChannel : public CTChannel
{
private:
        bool    blocking_m;
        int		maxto_m;		/* maximum timeout value. lazily computed. */
protected:
        int    sockfd_m;
        struct sockaddr_in      addr_m;

private:
    void CTTCPChannelInit();
        
public:
    static CTChannel *createChannel(const char *connectionInfo);
    /*
            PRE:    connection info should look like:
                            <host>;<port>
                            e.g. "foo.somewhere.com;4444"
    */
    
    CTTCPChannel(const char *host, unsigned short port);
    CTTCPChannel(struct sockaddr_in *addr);
    CTTCPChannel(int socketfd, struct sockaddr_in *addr);
    virtual ~CTTCPChannel();
    
    virtual bool openChannel(int timeoutSecs = 0);
    virtual void closeChannel();
    
    /* checking status of channel */
    virtual bool isReadable();
    virtual bool isWriteable();

    /* send/recv methods. */
    virtual CTChannelStatus sendData(const char *data, long int len, long int *sent);
    virtual CTChannelStatus sendData(const ulm_iovec_t *iov, int iovcnt, long int *sent);

    virtual CTChannelStatus receive(char *buffer, long int len, long int *lenrcvd);
    virtual CTChannelStatus receive(const ulm_iovec_t *iov, int iovcnt, long int *lenrcvd);
    virtual CTChannelStatus receiveAtMost(char *buffer, long int len, long int *lenrcvd);
                
    /* msg-oriented methods. */
    virtual CTChannelStatus getMessage(CTMessage **msg);
    virtual CTChannelStatus getPackedMessage(char **buffer, long int *msglen);
    
    virtual CTChannelStatus sendError(int errNumber);
    virtual CTChannelStatus sendMessage(CTMessage *msg);
    virtual CTChannelStatus sendPackedMessage(char *msg, long int msglen);
    
    /* accessor methods. */
    const char *className() {return "CTTCPChannel";}
    
    int socketfd() {return sockfd_m;}
    void setSocketfd(int sockfd);

    struct sockaddr_in address()  {return addr_m;}
    void setAddress(struct sockaddr_in *addr) {addr_m = *addr;}
    
    unsigned short port() {return ntohs(addr_m.sin_port);}
    void setPort(unsigned short port) {addr_m.sin_port = htons(port);}
    
    bool isBlocking() {return blocking_m;}
    bool setIsBlocking(bool tf);
    
    bool setTimeout(unsigned int secs);
    bool setMaximumTimeout(unsigned int secs);
    /*
     POST:	Attempts to set the socket timeout to secs.  If secs exceeds the upper limit, then
            the method will set to the maximum allowed timeout.
     */
    
    struct sockaddr_in *socketAddress() {return &addr_m;}
};


class CTTCPSvrChannel : public CTChannelServer
{
protected:
    CTTCPChannel 	**selected_m;
    int				selectcnt_m;
    fd_set 			readSet_m;
    int				maxfd_m;
    
public:
    CTTCPSvrChannel(unsigned short portn);
                
    /* server-side methods. */
    virtual bool setupToAcceptConnections();
    virtual CTChannelStatus acceptConnections(int timeoutSecs, CTChannel **client);
    CTChannelStatus acceptConnections(int timeoutSecs, CTTCPChannel *client);

    void setChannelsToCheckForRead(struct link_t *channels);
    bool channelsReadyForReading(int to_secs, int to_usecs);
    CTChannel **channelsReadable(struct link_t *channels, int *cnt);

};

#endif
