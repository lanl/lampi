/*
 * Copyright 2002-2004. The Regents of the University of
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>

#include "client/adminMessage.h"
#include "collective/coll_fns.h"
#include "ctnetwork/CTNetwork.h"
#include "internal/Private.h"
#include "internal/mpi.h"
#include "mem/ULMMallocMacros.h"
#include "os/atomic.h"          /* for fetchNadd */ 
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/MemFunctions.h"
#include "util/misc.h"
#include "util/parsing.h"

#define MAX_RETRY		100

typedef struct {
    int                 tag;
    CTMessage   *msg;
}  qitem_t;

typedef struct {
    int                 tag;
    int                 label;
    int                 rtype;
}  mkeys_t;


static void free_qitem(void *arg)
{
    qitem_t             *item = (qitem_t *)arg;
        
    if ( item )
    {
        item->msg->release();
        ulm_free2(item);
    }
}

#if ENABLE_CT

static int match_msg(void *arg, void *ctx)
{
    qitem_t             *item = (qitem_t *)arg;
    mkeys_t             *keys = (mkeys_t *)ctx;
        
    if ( ((keys->tag == item->tag) || (-1 == keys->tag))
         && ( ((unsigned int)(keys->label) == item->msg->sourceNode()) || (-1 == keys->label) )
         && ( ((keys->rtype) == item->msg->routingType()) || (0 == keys->rtype) )
        )
        return 1;
    else
        return 0;
}

#endif

/*
 * This routine scans through the list of hosts in hostList, and return
 * the index of the entry that matches nodeid
 */
int nodeIDToHostRank(int numHosts, HostName_t *hostList, int nodeid)
{
    int hostIndex = numHosts;
#if ENABLE_BPROC
    int j;

    for (j = 0; j < numHosts; j++) {
        if (atoi(hostList[j]) == nodeid) {
            hostIndex = j;
            break;
        }
    }
#endif
    return hostIndex;
}

/*
 * This routine scans through the list of hosts in hostList, and
 *   tries to find a match to client
 */
int socketToHostRank(int numHosts, HostName_t* hostList,
                   struct sockaddr_in *client, int assignNewId,
                   int *hostsAssigned)
{
    int j;
    int RetVal;
    struct hostent *TmpHost;
    int hostIndex=adminMessage::UNKNOWN_HOST_ID;
    TmpHost =
        gethostbyaddr((char *) &(client->sin_addr.s_addr), 4, AF_INET);
    if(TmpHost == NULL) {
        ulm_err(("socketToHostRank: unable to resolve: %s\n", inet_ntoa(client->sin_addr)));
        return hostIndex;
    }
    /* find order in list */
    for (j = 0; j < numHosts ; j++) {
        RetVal =
            strncmp(hostList[j], TmpHost->h_name,
                    strlen(TmpHost->h_name));
        if (RetVal == 0){
            /* if this host index already used - continue */
            if( assignNewId && (hostsAssigned[j]))
                continue;
            hostIndex=j;
            if( assignNewId )
                hostsAssigned[j]=1;
            break;
        }
    }                       /* end j loop */

    /* return host index */
    return hostIndex;
}
jmp_buf adminMessage::savedEnv;
struct sigaction adminMessage::oldSignals, adminMessage::newSignals;

void adminMessage::handleAlarm(int signal)
{
    if (signal == SIGALRM) {
        longjmp(savedEnv, 1);
    }
}

bool processNHosts(adminMessage * server, int rank, int tag)
{
    int errorCode;
    int nhosts = server->nhosts();
    bool returnValue = server->reset(adminMessage::SEND);
    returnValue = returnValue
        && server->pack(&nhosts, adminMessage::INTEGER, 1);
    returnValue = returnValue && server->send(rank, tag, &errorCode);
    return returnValue;
}



adminMessage::adminMessage()
{
    client_m = false;
    server_m = false;
    connectInfo_m = NULL;
    msgQueue_m = NULL;
    scatterHosts_m = NULL;
    scatterLen_m = NULL;
    daemon_m = NULL;
    tcpChannel_m = NULL;
    svrChannel_m = NULL;
    netconn_m = NULL;
    recvlens_m = NULL;

    nhosts_m = 0;
    hostRank_m = -2;
    sendBufferSize_m = DEFAULTBUFFERSIZE;
    recvBufferSize_m = DEFAULTBUFFERSIZE;
    sendBuffer_m = (unsigned char *)ulm_malloc(sendBufferSize_m);
    recvBuffer_m = (unsigned char *)ulm_malloc(recvBufferSize_m);

    if (!sendBuffer_m || !recvBuffer_m) {
        ulm_exit((-1, "adminMessage::adminMessage unable to allocate %d bytes for send/receive buffers\n",
                  sendBufferSize_m));
    }
    sendOffset_m = recvOffset_m = 0;
    hostname_m[0] = hostname_m[MAXHOSTNAMESIZE - 1] = '\0';
    socketToServer_m = -1;
    serverSocket_m = -1;
    largestClientSocket_m = -1;
    lastRecvSocket_m = -1;
    hint_m = 0;

    for (int i = 0; i < MAXSOCKETS; i++) {
        clientSocketActive_m[i] = false;
        ranks_m[i] = -1;
    }

    for (int i = 0; i < NUMMSGTYPES; i++) {
        callbacks_m[i] = (callbackFunction)0;
    }

    /* all signals will be blocked while handleAlarm is running */
    newSignals.sa_handler = &handleAlarm;
    sigfillset(&newSignals.sa_mask);
    newSignals.sa_flags = 0;

    /* initialize collective resources */
    groupHostData_m = (admin_host_info_t *)NULL;
    barrierData_m = (swBarrierData *)NULL;
    sharedBuffer_m = 0;
    lenSharedMemoryBuffer_m = 0;
    syncFlag_m = (int *)NULL;
    localProcessRank_m = -2;
    hostCommRoot_m = -2;
    collectiveTag_m = -1;

}



/* default destructor */
adminMessage::~adminMessage() 
{

    /* free collective resources */
    if( groupHostData_m )
        ulm_free(groupHostData_m);
        
    if ( daemon_m )
    {
        daemon_m->stop();
        delete daemon_m;
    }
        
    if ( netconn_m )
    {
        netconn_m->disconnect();
        delete netconn_m;
    }
        
    if ( svrChannel_m )
    {
        svrChannel_m->channel()->closeChannel();
        delete svrChannel_m;
    }
        
    free_double_carray(connectInfo_m, nhosts_m);
    freeall_with(msgQueue_m, free_qitem);
        
    terminate();
}


bool adminMessage::clientInitialize(int *authData, char *hostname, int port) 
{
#if ENABLE_CT
    CTTCPSvrChannel         *chnl;
#endif
                
    client_m = true;
    // already null-terminated in last byte for sure...
    // hostname_m is the name of host containing mpirun.
    strncpy(hostname_m, hostname, MAXHOSTNAMESIZE - 1);
    // store port and authentication handshake data for later use
    port_m = port;
    for (int i = 0; i < 3; i++) {
        authData_m[i] = authData[i];
    }

#if ENABLE_CT
    chnl = new CTTCPSvrChannel(0);
    daemon_m = new CTServer((CTChannelServer  *)chnl);
    daemon_m->setDelegate(this);

    if ( false == daemon_m->start() )
    {
        ulm_err( ("Error: Can't start daemon server. Exiting...\n") );
        return false;
    }
#endif

    return true;
}


bool adminMessage::connectToRun(int nprocesses, int hostrank, int timeout)
{
    int             sockbuf = 1, tag = INITMSG;
    int             authOK;
    struct          sockaddr_in server;
    ulm_iovec_t iovecs[6], riov[1];
    pid_t           mypid;
    long int        chlen;
    char            buf[100], lhost[50];
    bool            success = true;
    unsigned short  svrport;
    CTTCPChannel    *chnl;
    CTChannelStatus status;
    struct hostent *serverHost;

    serverHost = gethostbyname(hostname_m);
#if ENABLE_BPROC
    if (serverHost == (struct hostent *)NULL) {
        /* can't find server's address via hostname_m --> use BPROC utilities to find the master */
        int size = sizeof(struct sockaddr);
        if (bproc_nodeaddr(BPROC_NODE_MASTER, (struct sockaddr *)&server, &size) != 0) {
            ulm_err(("adminMessage::clientConnect error returned from the bproc_nodeaddr call :: errno - %d\n",errno));
            return false;
        }
    }
    else {
        memcpy( (char*)&server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
    }
#else
    if (serverHost == (struct hostent *)NULL) {
        ulm_err(("adminMessage::clientConnect gethostbyname(\"%s\") failed (h_errno = %d)!\n", hostname_m, h_errno));
        return false;
    }
    memcpy( (char*)&server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
#endif
    server.sin_port = htons((unsigned short)port_m);
    server.sin_family = AF_INET;

    // stagger our connection establishment back to mpirun
    if (hostrank > 0) {
        usleep((hostrank % 1000) * 1000);
    }

    // create channel to connect to mpirun
    chnl = new CTTCPChannel(&server);
    setsockopt(chnl->socketfd(), IPPROTO_TCP, TCP_NODELAY, &sockbuf, sizeof(int));
    chnl->setTimeout(timeout);

    // set up response iovec to receive from mpirun
    riov[0].iov_base = &authOK;
    riov[0].iov_len = sizeof(authOK);

    // set up iovec for sending to mpirun
    mypid =  getpid();
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t)sizeof(int);
    iovecs[1].iov_base = authData_m;
    iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
    iovecs[2].iov_base = &hostrank;
    iovecs[2].iov_len = (ssize_t)sizeof(int);
    iovecs[3].iov_base = &nprocesses;
    iovecs[3].iov_len = (ssize_t)sizeof(int);
    iovecs[4].iov_base = &mypid;
    iovecs[4].iov_len = (ssize_t)sizeof(pid_t);

    // send connection info for the daemon's server
    svrport =  ((CTTCPChannel *)daemon_m->channel())->port();
    gethostname(lhost, sizeof(lhost));
    sprintf(buf, "%s;%u", lhost, svrport);
    iovecs[5].iov_base = buf;
    iovecs[5].iov_len = strlen(buf)+1;
        
    // attempt to connect
    if  ( true == chnl->openChannel(timeout) )
    {
        // send auth data and basic daemon info
        status = chnl->sendData(iovecs, 6, &chlen);
                
        // get ok status
        authOK = false;
        if ( kCTChannelOK == status )
        {
            status = chnl->receive(riov, 1, &chlen);
            if ( kCTChannelOK != status )
            {
                ulm_err( ("daemon %d: failed getting auth data from mpirun.\n", mypid) );
                success = false;
            }
        }
        else
        {
            ulm_err( ("daemon %d: failed sending data to mpirun.\n", mypid) );
            success = false;
        }
                
        if ( true != authOK )
        {
            ulm_err( ("daemon %d: failed authorization.\n", mypid) );
            success = false;
        }
    }
    else
    {
        ulm_err(("adminMessage::clientConnect connect to server %s TCP port %d failed!\n", hostname_m, port_m));
        success = false;
    }
    delete chnl;
    
    return success;
}



bool adminMessage::clientConnect(int nprocesses, int hostrank, int timeout) 
{
    int sockbuf = 1, tag = INITMSG, connect_retry;
    int ok, recvAuthData[3];
    struct sockaddr_in server;
    ulm_iovec_t iovecs[5];
    pid_t myPID;
    struct hostent *serverHost;

#if ENABLE_CT
    return connectToRun(nprocesses, hostrank, timeout);
#endif

    socketToServer_m = socket(AF_INET, SOCK_STREAM, 0);
    if (socketToServer_m < 0) {
        ulm_err(("adminMessage::clientConnect unable to open TCP/IP socket!\n"));
        return false;
    }
    setsockopt(socketToServer_m, IPPROTO_TCP, TCP_NODELAY, &sockbuf, sizeof(int));
    serverHost = gethostbyname(hostname_m);

#if ENABLE_BPROC
    if (serverHost == (struct hostent *)NULL) {
        /* can't find server's address via hostname_m --> use BPROC utilities to find the master */
        int size = sizeof(struct sockaddr);
        if (bproc_nodeaddr(BPROC_NODE_MASTER, (struct sockaddr *)&server, &size) != 0) {
            ulm_err(("adminMessage::clientConnect error returned from the bproc_nodeaddr call :: errno - %d\n",errno));
            return false;
        }
    }
    else {
        memcpy( (char*)&server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
    }
#else
    if (serverHost == (struct hostent *)NULL) {
        ulm_err(("adminMessage::clientConnect gethostbyname(\"%s\") failed (h_errno = %d)!\n", hostname_m, h_errno));
        return false;
    }
    memcpy( (char*)&server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
#endif
    server.sin_port = htons((unsigned short)port_m);
    server.sin_family = AF_INET;


    // stagger our connection establishment back to mpirun
    if (hostrank > 0) {
        usleep((hostrank % 1000) * 1000);
    }


    if (timeout > 0) {
        if (sigaction(SIGALRM, &newSignals, &oldSignals) < 0) {
            ulm_err(("adminMessage::clientConnect unable to install SIGALRM handler!\n"));
            return false;
        }
        if (setjmp(savedEnv) != 0) {
            ulm_err(("adminMessage::clientConnect %d second timeout to %s exceeded!\n", timeout, hostname_m));
            return false;
        }
        alarm(timeout);
    }

    // attempt to connect
    for ( connect_retry = 0; connect_retry < MAX_RETRY ; connect_retry++ )
    {
        if (connect(socketToServer_m, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
        {
            if ( (ETIMEDOUT == errno) || (ECONNREFUSED == errno) )
            {
                usleep(10);
            }
            else
            {
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
                }
                ulm_err(("adminMessage::clientConnect connect to server %s TCP port %d failed!\n", hostname_m, port_m));
                close(socketToServer_m);
                socketToServer_m = -1;
                return false;                
            }
        }
        else
        {
            break;
        }
    }

    // now do the authorization handshake...send info
    myPID=getpid();
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t)sizeof(int);
    iovecs[1].iov_base = authData_m;
    iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
    iovecs[2].iov_base = &hostrank;
    iovecs[2].iov_len = (ssize_t)sizeof(int);
    iovecs[3].iov_base = &nprocesses;
    iovecs[3].iov_len = (ssize_t)sizeof(int);
    iovecs[4].iov_base = &myPID;
    iovecs[4].iov_len = (ssize_t)sizeof(pid_t);
    if (ulm_writev(socketToServer_m, iovecs, 5) != 
        ( 6*sizeof(int) + sizeof(pid_t)  ) ){
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
        }
        ulm_err(("adminMessage::clientConnect write to server socket failed!\n"));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }

    // check for OK message
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t)sizeof(int);
    iovecs[1].iov_base = recvAuthData;
    iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
    iovecs[2].iov_base = &ok;
    iovecs[2].iov_len = (ssize_t)sizeof(int);
    if (ulm_readv(socketToServer_m, iovecs, 3) != 5*sizeof(int)) {
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
        }
        ulm_err(("adminMessage::clientConnect read from server socket failed!\n"));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }

    // check info from server
    if (tag != INITOK) {
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
        }
        ulm_err(("adminMessage::clientConnect did not receive expected INITOK message (tag = %d)!\n", tag));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }

    for (int i = 0; i < 3; i++) {
        if (recvAuthData[i] != authData_m[i]) {
            if (timeout > 0) {
                alarm(0);
                sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
            }
            ulm_err(("adminMessage::clientConnect did not receive matching authorization data!\n"));
            close(socketToServer_m);
            socketToServer_m = -1;
            return false;
        }
    }

    if (!ok) {
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
        }
        ulm_err(("adminMessage::clientConnect server does not give us the go-ahead to continue!\n"));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }

    if (timeout > 0) {
        alarm(0);
        sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
    }
    return true;
}



bool adminMessage::clientNetworkHasLinked()
{
    if ( daemon_m )
    {
        return daemon_m->networkHasLinked();        
    }
    else
        return false;
}



bool adminMessage::serverInitialize(int *authData, int nprocs, int *port)
{
#ifdef __linux__
    socklen_t namelen;
#else
    int namelen;
#endif
    int sockbuf = 1;
    struct sockaddr_in socketInfo;

#if ENABLE_CT
    svrChannel_m = new CTTCPSvrChannel(0);
    setsockopt(((CTTCPChannel *)svrChannel_m->channel())->socketfd(),
               IPPROTO_TCP, TCP_NODELAY, &sockbuf, sizeof(int));
    if ( false == svrChannel_m->setupToAcceptConnections() )
    {
        ulm_err( ("Error: Can't setup for connections..\n") );
        return false;
    }
    *port = ((CTTCPChannel *)svrChannel_m->channel())->port();
#endif
        
    namelen = sizeof(socketInfo);
    server_m = true;
    totalNProcesses_m = nprocs;
    for (int i = 0; i < 3; i++) {
        authData_m[i] = authData[i];
    }

#if ENABLE_CT == 0
    registerCallback(NHOSTS, &processNHosts);

    serverSocket_m = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_m < 0) {
        ulm_err(("adminMessage::serverInitialize unable to create server socket!\n"));
        return false;
    }
    setsockopt(serverSocket_m, IPPROTO_TCP, TCP_NODELAY, &sockbuf, sizeof(int));

    bzero(&socketInfo, sizeof(struct sockaddr_in));
    socketInfo.sin_family = AF_INET;
    socketInfo.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket_m, (struct sockaddr *)&socketInfo, sizeof(struct sockaddr_in)) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to bind server TCP/IP socket!\n"));
        return false;
    }

    if (getsockname(serverSocket_m, (struct sockaddr *)&socketInfo, &namelen) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to get server socket information!\n"));
        return false;
    }
    *port = (int)ntohs(socketInfo.sin_port);
    if (listen(serverSocket_m, SOMAXCONN) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to listen on server socket!\n"));
        return false;
    }
#endif
        
    return true;
}


void adminMessage::sortNodeLabels(int *labels2Rank)
{
    // reorder node labeling so that the node label is the same as the hostrank
    char        *ptr;
    int         i, j, rank;
    
    for ( i = 0; i < nhosts_m; i++ )
    {
        if ( labels2Rank[i] != i )
        {
            rank = labels2Rank[i];
            while ( labels2Rank[rank] != i )
                rank = labels2Rank[rank];
            // swap the connection info so that connectInfo_m[i] is for hostrank i
            ptr = connectInfo_m[i];
            connectInfo_m[i] = connectInfo_m[rank];
            connectInfo_m[rank] = ptr;

            j = labels2Rank[i];
            labels2Rank[i] = labels2Rank[rank];
            labels2Rank[rank] = j;
        }
    }

}

bool adminMessage::collectDaemonInfo(int* procList, HostName_t* hostList, int numHosts,
                                     int timeout)
{
    int             np = 0, tag, hostrank, nprocesses, recvAuthData[3], authOK;
    int             cnt, hostcnt, *ranks;
    ulm_iovec_t iovecs[6], riov[1];
    int             *hostsAssigned = 0, daemon_to;
    long int        rcvdlen, sent;
    pid_t           daemonPid;
    char            buffer[100];
    bool            success, didtimeout;
    CTChannelStatus status;
    CTTCPChannel            *daemon;
    struct timeval          endtime, curtime;

#if ENABLE_BPROC == 0
    int assignNewId;
#endif
        
    /* initialization "stuff" */
    hostsAssigned = NULL;
    ranks = NULL;
    if( numHosts > 0 ) 
    {
        /*
          NOTE: the numHosts passed may not coincide with actual number.
          E.g. on Q, this value is 1.
        */
        hostsAssigned = (int *)ulm_malloc(numHosts*sizeof(int));
        connectInfo_m = (char **)ulm_malloc(numHosts*sizeof(char *));
        ranks = (int *)ulm_malloc(numHosts*sizeof(int));
        scatterHosts_m = (unsigned char **)ulm_malloc(numHosts*sizeof(unsigned char *));
        scatterLen_m = (int *)ulm_malloc(numHosts*sizeof(int));
        if( !hostsAssigned || !connectInfo_m || !ranks
            || !scatterHosts_m || !scatterLen_m ) 
        {
            ulm_err((" Unable to allocate memory for hostsAssigned list \n"));
            ulm_free2(hostsAssigned);
            ulm_free2(connectInfo_m);
            ulm_free2(scatterHosts_m);
            ulm_free2(scatterLen_m);
            ulm_free2(ranks);
            return false;
        }

        
        bzero(scatterHosts_m, numHosts*sizeof(unsigned char *));
        bzero(connectInfo_m, numHosts*sizeof(char *));
        bzero(scatterLen_m, numHosts*sizeof(int));
        for(int i=0 ; i < numHosts ; i++ )
        {
            hostsAssigned[i] = 0;
            ranks[i] = -1;
        }
    }

    daemon_to = 0;
    if (timeout > 0) 
    {
        daemon_to = timeout / numHosts;
    }
    ((CTTCPChannel *)svrChannel_m->channel())->setMaximumTimeout(daemon_to);
        
    // wait for all clients to connect

    // initialize processCount and daemonPIDs
    for(cnt=0 ; cnt < MAXSOCKETS ; cnt++)
    {
        processCount[cnt] = (int)0;
        daemonPIDs_m[cnt] = (size_t)0;
    }


    // set up the authorization handshake...receive info
    riov[0].iov_base = &authOK;
    riov[0].iov_len = sizeof(authOK);
        
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t)sizeof(int);
    iovecs[1].iov_base = recvAuthData;
    iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
    iovecs[2].iov_base = &hostrank;
    iovecs[2].iov_len = (ssize_t)sizeof(int);
    iovecs[3].iov_base = &nprocesses;
    iovecs[3].iov_len = (ssize_t)sizeof(int);
    iovecs[4].iov_base = &daemonPid;
    iovecs[4].iov_len = (ssize_t)sizeof(pid_t);
    iovecs[5].iov_base = buffer;
    iovecs[5].iov_len = 100;

    gettimeofday(&endtime, NULL);
    endtime.tv_sec += timeout;
        
    success = true;
    didtimeout = false;
    hostcnt = 0;

    daemon = new CTTCPChannel((struct sockaddr_in *)NULL);
    while ( (np < totalNProcesses_m) && (true == success) )
    {
        status = svrChannel_m->acceptConnections(daemon_to, daemon);
                
        if ( kCTChannelOK != status )
        {
            if ( kCTChannelTimedOut == status )
            {
                ulm_err( ("Timed out while waiting for connections.\n")  );
            }
            else
                ulm_err( ("Error: while waiting for connections. status = %d\n", status) );
                                
            // check for timeout
            gettimeofday(&curtime, NULL);
            if ( curtime.tv_sec > endtime.tv_sec )
            {
                success = false;
                didtimeout = true;
            }
                        
            continue;
        }
                        
        daemon->setMaximumTimeout(daemon_to);
        // get daemon info: 
        //      tag: auth data : host rank : nprocesses : daemon PID : connection info string           
        status = daemon->receive(iovecs, 6, &rcvdlen);
        if ( kCTChannelOK != status )
        {
            if ( kCTChannelTimedOut == status )
            {
                ulm_err( ("Timed out while receiving daemon info.\n")  );
            }
            else
                ulm_err( ("Error: while receiving daemon info. status = %d\n", status) );
                                
            success = false;
            // check for timeout
            gettimeofday(&curtime, NULL);
            if ( curtime.tv_sec > endtime.tv_sec )
            {
                didtimeout = true;
            }                       
            continue;
        }
               
#if ENABLE_BPROC
        /* BPROC node id from remote bproc_currnode() call must be translated */
        hostrank = nodeIDToHostRank(numHosts, hostList, hostrank);
        if (hostrank == numHosts) {
            ulm_err(("Error: adminMessage::serverConnect nodeIDToHostRank failed!\n"));
            success = false;
        }
#else
        // set hostrank. For RMS/Q hostrank should not be UNKNOWN_HOST_ID.
        if( hostrank == UNKNOWN_HOST_ID ) {
            assignNewId = 1;
            hostrank = socketToHostRank(numHosts,hostList, daemon->socketAddress(),
                                      assignNewId,hostsAssigned);
            if( hostrank == UNKNOWN_HOST_ID ){
                ulm_err(("Error: adminMessage::serverConnect UNKNOWN_HOST_ID\n"));
                success = false;
            }
        }
#endif

        if ( true == success )
        {
            // cache process count
            if(nprocesses != UNKNOWN_N_PROCS )
            {
                processCount[hostrank]=nprocesses;
            }
            else {
                processCount[hostrank]=procList[hostrank];
                nprocesses=procList[hostrank];
            }
                                                
            // check the message tag
            if (tag != INITMSG)
            {
                ulm_err(("adminMessage::serverConnect client socket received tag that"
                         " is not INITMSG (tag = %d)\n", tag));
                success = false;
                continue;
            }
        
            // check the authorization data
            for (int i = 0; i < 3; i++)
            {
                if (recvAuthData[i] != authData_m[i]) 
                {
                    ulm_err(("adminMessage::serverConnect client socket authorization data bad!\n"));
                    success = false;
                }
            }
        
            // check the global host rank to make sure it is unique
            for (int i = 0; (i < hostcnt) && success; i++)
            {
                if ( ranks[i] == hostrank ) 
                {
                    ulm_err(("adminMessage::serverConnect duplicate host rank %d\n", hostrank));
                    success = false;
                }
            }
            
            authOK = false;
            // store the information about this socket
            if ( success ) 
            {
                cnt = (hostrank > hostcnt) ? hostrank : hostcnt;
                if ( cnt > (numHosts - 1) )
                {
                    // make good estimate about resize value
                    numHosts = cnt + (totalNProcesses_m >> 2);

                    hostsAssigned = (int *)realloc(hostsAssigned, numHosts*sizeof(int));
                    ranks = (int *)realloc(ranks, numHosts*sizeof(int));
                    connectInfo_m = (char **)realloc(connectInfo_m, numHosts*sizeof(char *));
                    scatterHosts_m = (unsigned char **)realloc(scatterHosts_m, numHosts*sizeof(unsigned char *));
                    scatterLen_m = (int *)realloc(scatterLen_m, numHosts*sizeof(int));
                    if( !hostsAssigned || !connectInfo_m || !ranks
                        || !scatterHosts_m || !scatterLen_m )
                    {
                        ulm_err((" Unable to allocate memory for hostsAssigned list \n"));
                        ulm_free2(hostsAssigned);
                        ulm_free2(connectInfo_m);
                        ulm_free2(scatterHosts_m);
                        ulm_free2(scatterLen_m);
                        ulm_free2(ranks);
                        return false;
                    }
                }
                connectInfo_m[hostrank] = strdup(buffer);
                ranks[hostcnt] = hostrank;
                                
                hostcnt++;
                np += nprocesses;

                // cache daemon PID
                daemonPIDs_m[hostrank] = daemonPid;
                authOK = true;
            }
        
            status = daemon->sendData(riov, 1, &sent);
            if ( kCTChannelOK != status )
            {
                if ( kCTChannelTimedOut == status )
                {
                    ulm_err( ("Timed out while sending authOK to daemon.\n")  );
                }
                else
                    ulm_err( ("Error: while sending authOK to daemon. status = %d\n", status) );
                                        
                success = false;
                // check for timeout
                gettimeofday(&curtime, NULL);
                if ( curtime.tv_sec > endtime.tv_sec )
                {
                    didtimeout = true;
                }                       
            }

        }       // if ( true == success )
        daemon->closeChannel();
    }       // while ( (np < totalNProcesses_m) && (false == done) )

    
    nhosts_m = hostcnt;
    
    // check on timeout status
    if ( true == didtimeout )
    {
        ulm_err(("adminMessage::serverConnect timeout %d exceeded --"
                 " %d client sockets account for %d processes!\n",
                 timeout, hostcnt, np));
	ulm_err(("\nmpirun was unable to start you application.\n"
		"This may be caused by several things \n"
		"- the application may not exist on the remote node\n"
		"- the application may not be executable on the remote node\n"
		"- the loader may not be able to find the dynamic libraries \n"
		"needed to run the application.\n"
		"Check to see that your remote LD_LIBRARY_PATH points to the \n"
		"correct MPI library, and to any other dynamic libraries your \n"
		"application needs. \n"
		"- make sure that the mpirun you are using goes with the MPI \n"
		"library you are trying to use.\n"));
		
        for (int i = 0; i < nhosts_m; i++) {
            if (ranks[i] >= 0)
            {
                char    hostn[50];
                
                if ( true == peerName(i, hostn, 50, true))
                {
                    ulm_err(("\thostrank %d peer info: IP %s process count %d PID %ld\n", i,
                             hostn, processCount[i], (long)daemonPIDs_m[i]));
                }
            }
        }
    }

    ulm_free2(hostsAssigned);
    ulm_free2(ranks);

    svrChannel_m->channel()->closeChannel();
    
    return success;
}



bool adminMessage::linkNetwork()
{
    CTTCPChannel            *chnl;
    unsigned int            ctrl, *labels, i;
    CTMessage               *msg;
    CTChannelStatus         status;
    
    /* find connection for node 0 and send linkup info. */
    if ( NULL ==  netconn_m )
    {
        timing_stmp = second();
        sprintf(timing_out[timing_scnt++], "Linking network (t = %lf).\n", timing_stmp - timing_cur);
        timing_cur = timing_stmp;

        // linking the network MUST begin with node 0.  Once connected, then
        // establish connection to host rank 0 and not node 0.
        // ASSERT: node 0 is hostrank 0
        labels = (unsigned int *)ulm_malloc(nhosts_m * sizeof(unsigned int));
        if ( NULL == labels )
        {
            ulm_err( ("Error: Unable to alloc labels array.\n") );
            return false;
        }
        
        //  connect to host rank 0 and not node 0.
        chnl = (CTTCPChannel *)CTChannel::createChannel("CTTCPChannel", connectInfo_m[0]);
        if ( !chnl )
        {
            ulm_err( ("Error: Can't create channel to node 0.\n") );
            return false;
        }
        netconn_m = new CTClient(chnl);

        if ( false == netconn_m->connect(10) )
        {
            ulm_err( ("Unable to connect to node 0 ( connectInfo = %s).\n", connectInfo_m[0]) );
            return false;
        }
        for ( i = 0; i < (unsigned int)nhosts_m; i++ )
            labels[i] = i;

        msg = CTServer::linkNetworkMessage(CTNode::kHypercube, nhosts_m, labels, (const char **)connectInfo_m);
        ctrl = HypercubeNode::initialControlData(nhosts_m);
        status = netconn_m->sendMessage(msg, sizeof(ctrl), (char *)&ctrl);

        msg->release();
        ulm_free2(labels);

        if ( kCTChannelOK != status)
        {
            ulm_err( ("Unable to broadcast msg to link network. status = %d.\n", status) );
            return false;
        }

        timing_stmp = second();
        sprintf(timing_out[timing_scnt++], "Done linking network (t = %lf).\n", timing_stmp - timing_cur);
        timing_cur = timing_stmp;

    }

    return true;
}
        


/*
 *      CTDelegate implementations
 */
         
#if ENABLE_CT
void adminMessage::messageDidArrive(CTServer *server, CTMessage *msg)
{
    qitem_t         *item;
    const char      *data;
    link_t          *litem;
        
    // ASSERT: msg data format:
    // <tag (int)><msg content>
    msg->retain();
    item = (qitem_t *)ulm_malloc(sizeof(qitem_t));
    litem = newlink(item);
    if ( item && litem )
    {
        data = msg->data();
        memcpy(&(item->tag), data, sizeof(item->tag));
        item->msg = msg;
        qlock_m.lock();
        msgQueue_m = addend(msgQueue_m, litem);
        qlock_m.unlock();
    }
    else
    {
        ulm_err( ("Error: Unable to create queue item for msg.\n") );
        free_qitem(item);
        ulm_free2(litem);
                
    }
}

void adminMessage::nodeDidFail(/* node specific info. */)
{
}
#endif


int adminMessage::exchange(void *sendbuf, void *recvbuf, ssize_t bytesPerHost)
{
    /*
      PRE:    recvbuf is of size nhosts_m * bytesPerHost.
      sendbuf is of size bytesPerHost.
      POST:   performs the same functionally as allgather, but only involves the daemon process
      and not the user processes.
    */
#if ENABLE_CT
    if ( kCTChannelOK == daemon_m->allgather(nhosts_m, bytesPerHost,
                                             (char *)sendbuf, (char *)recvbuf)  )
        return ULM_SUCCESS;
    else
        return ULM_ERROR;
#else
    return 0;
#endif
}


/* this primitive implementation of allgather handles only contiougs
 *   data.  In this implementation, each host aggregates it's data
 *   and sends it to the mpirun.  Once mpirun has gathered all the 
 *   data it broadcasts this data to all hosts.
 *     sendbuf - source buffer
 *     recvbug - destination buffer
 *     bytesPerProc - number of bytes each process contributes to the
 *        collective operation
 */
int adminMessage::allgather(void *sendbuf, void *recvbuf, 
                            ssize_t bytesPerProc)
{
    ssize_t         recvCount = bytesPerProc;
    int                 returnCode = ULM_SUCCESS,typeTag,*dataArrivedFromHost;
    int             nHostsArrived,host,hst;
#if ENABLE_CT
    int                 bytesToSend;
#endif
    ssize_t         totalBytes, bytesToCopy = 0;
    void *RESTRICT_MACRO recvBuff = recvbuf;
    void *RESTRICT_MACRO sendBuff = sendbuf;
    bool                bReturnValue;
    void            *src,*dest;
    size_t          *aggregateData, offsetIntoAggregateData;
    recvResult  recvReturnCode;
    long long       tmpTag;
    int                 nLocalProcs, nStripesOnThisHost, *bytesLeftPerHost;

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = get_base_tag(1);

    int maxLocalProcs=0;
    for (host = 0; host < nhosts_m ; host++) {
        int nLocalProcs = groupHostData_m[host].nGroupProcIDOnHost;
        if( nLocalProcs > maxLocalProcs )
            maxLocalProcs=nLocalProcs;
    }
    ssize_t localStripeSize=lenSharedMemoryBuffer_m/nhosts_m;
    if(localStripeSize < 0 ) {
        ulm_err(("Error: adminMessage::allgather, localStripeSize = %lld\n",
                 (long long)localStripeSize));
        returnCode = ULM_ERROR;
        return returnCode;
    }
    ssize_t perRankStripeSize=localStripeSize/maxLocalProcs;
    if(perRankStripeSize < 0 ) {
        ulm_err(("Error: adminMessage::allgather, perRankStripeSize = %lld\n",
                 (long long)perRankStripeSize));
        returnCode=ULM_ERROR;
        return returnCode;
    }

    if ( NULL == (bytesLeftPerHost = (int *)ulm_malloc(sizeof(int)*nhosts_m)) )
        return ULM_ERROR;
        
    int numStripes = 0;
    for (host = 0; host < nhosts_m ; host++) {
        nLocalProcs = groupHostData_m[host].nGroupProcIDOnHost;
        totalBytes = nLocalProcs * recvCount;
        bytesLeftPerHost[host] = totalBytes;
#if ENABLE_CT
        recvlens_m[host] = (bytesPerProc > perRankStripeSize) ? 
            nLocalProcs*perRankStripeSize : nLocalProcs*bytesPerProc;
#endif
        if (totalBytes == 0) {
            nStripesOnThisHost = 1;
            numStripes = 1;
        } else {
            nStripesOnThisHost = ((totalBytes - 1) / localStripeSize) + 1;
            if (nStripesOnThisHost > numStripes)
                numStripes = nStripesOnThisHost;
        }
    }

    /* client code */
    if( server_m )
        goto ServerCode;

    /*  while loop over message stripes.
     *
     *  At each stage a shared memory buffer is filled in by the processes
     *    on this host.  This data is sent to other hosts, and read by the
     *    local processes.
     *
     *  The algorithm used to exchange data between inolves partitioning the hosts
     *    into two groups, one the largest number of host ranks that is a
     *    power of 2 (called the exchange group of size exchangeSize) and the remainder.
     *    The data exchange pattern is:
     *    - the remainder group hosts send their data to myHostRank-exchangeSize
     *    - the exchange group hosts exchange data between sets of these
     *    hosts.  The size of the first set is 2, then 4, then 8, etc. with
     *    each host in the lower half of the set exchanging data with one in
     *    the upper half of the set.
     *    - the remainder group hosts get their data to myHostRank-exchangeSize
     */

    for (int stripeID = 0; stripeID < numStripes; stripeID++) {

        /* write data to source buffer, if this process contributes to this
         *   stripeID
         */
        ssize_t bytesAlreadyCopied = stripeID * perRankStripeSize;
        ssize_t leftToCopy = recvCount - bytesAlreadyCopied;
        bytesToCopy = leftToCopy;
        if (bytesToCopy > perRankStripeSize)
            bytesToCopy = perRankStripeSize;

        
        if (bytesToCopy > 0 && localProcessRank_m != DAEMON_PROC) {

            /*
             * Fill in shared memory buffer
             */
            size_t bytesAlreadyCopied = stripeID * perRankStripeSize;
            size_t offsetIntoSharedBuffer = bytesToCopy*localProcessRank_m;

            src = (void *) ((char *) sendBuff + bytesAlreadyCopied);
            dest = (void *) ((char *) sharedBuffer_m + offsetIntoSharedBuffer);

            /* copy data */
            MEMCOPY_FUNC(src, dest, bytesToCopy);
        
        }

            
        
        /* set flag for releasing after interhost data exchange -
         *   must be set before this barrier to avoid race conditions.
         *   saves a barrier call after the data exchange
         */
        if (localProcessRank_m == hostCommRoot_m) {
            *syncFlag_m= 0;
        }

        /* don't send data until all have written to the shared buffer
         */
        localBarrier();

        /*
         * Read the shared memory buffer - interhost data exchange
         */

        if ( localProcessRank_m == hostCommRoot_m ) 
        {

            /* 
             * simple interhost accumlation of data 
             */
        
#if ENABLE_CT
            /* send data to mpirun */
            bReturnValue = reset(adminMessage::SEND);
            if( !bReturnValue )
                return ULM_ERROR;
            bytesToSend = bytesToCopy*groupHostData_m[hostRank_m].nGroupProcIDOnHost;
                                
            totalBytes = totalNProcesses_m*bytesToCopy;
            bReturnValue = pack(sharedBuffer_m, BYTE, bytesToSend);
            if( !bReturnValue )
                return ULM_ERROR;
                
            daemon_m->allgatherv(nhosts_m, bytesToSend, (char *)sendBuffer_m, 
                                 recvlens_m, (char *)sharedBuffer_m);
            // determine the size of data that each host will send during each stripe iteration
            for (host = 0; host < nhosts_m ; host++)
            {
                nLocalProcs = groupHostData_m[host].nGroupProcIDOnHost;
                bytesLeftPerHost[host] -= recvlens_m[host];
                if ( bytesLeftPerHost[host] < nLocalProcs*perRankStripeSize )
                    recvlens_m[host] = bytesLeftPerHost[host];
            }
#else
            /* send data to mpirun */
            bReturnValue=reset(adminMessage::SEND);
            if( !bReturnValue )
                return ULM_ERROR;
            bReturnValue=pack(&tag,LONGLONG,1);
            if( !bReturnValue )
                return ULM_ERROR;
            bReturnValue=pack(sharedBuffer_m,BYTE,
                              bytesToCopy*groupHostData_m[hostRank_m].nGroupProcIDOnHost);
            if( !bReturnValue )
                return ULM_ERROR;
                                
            totalBytes = totalNProcesses_m*bytesToCopy;
            bReturnValue = send(-1, adminMessage::ALLGATHER, &returnCode);
            if( !bReturnValue )
                return returnCode;
        
            /* receive the data from mpirun */
            /* compute how much data to receive */
            reset(adminMessage::RECEIVE, totalBytes);
            bReturnValue = receive(-1,&typeTag,&returnCode);
            if( !bReturnValue )
                return returnCode;
            if(typeTag != adminMessage::ALLGATHER ){
                ulm_err(("Error: unexpected tag in allgatherMultipleStripes.\n"
                        "\ttag received %d expected tag %d\n",
                        typeTag,adminMessage::ALLGATHER));
                return returnCode;
            }
        
            bReturnValue = unpack(sharedBuffer_m, BYTE, totalBytes);
            if( !bReturnValue ){
                ulm_err(("Error: unpack returned error in allgatherMultipleStripes\n"));
                return returnCode;
            }
        
#endif
            /* memory barrier to ensure that all data has been written before setting flag */
            mb();
            
            /* set flag indicating data exchange is done */
            *syncFlag_m = 1;


        }
        /* spin until all data has been received */
        while(!(*syncFlag_m))
            ;

        /* Unpack the incoming data.  Data arrives sorted by host
         * index. e.g. host0, host1, host2, ...  within a given
         * host's data, this is arranged by local rank */

#if ENABLE_CT
        /* the daemon process does not need this information */
        if( recvBuff ) {
            ssize_t sharedMemBufOffset;
            
            sharedMemBufOffset = 0;
            //ASSERT: nhosts_m == number of nodes 
            for ( host = 0; (unsigned int)host <  daemon_m->node()->numberOfNodes(); host++ ) 
            {
                // sharedMemBufOffset points to start of host data in buffer
                for (int lProc=0 ; lProc < groupHostData_m[host].nGroupProcIDOnHost ; 
                     lProc++ )
                {
                    int proc = groupHostData_m[host].groupProcIDOnHost[lProc];
                    
                    // (recvBuff + proc*bytesPerProc) points to start of proc's data in dest buffer
                    // sharedBuffer_m + sharedMemBufOffset points to start of host's data in shared buffer
                    dest = (void *) ((char *) recvBuff + bytesAlreadyCopied + proc*bytesPerProc);
                    src = (void *) ((char *) sharedBuffer_m + sharedMemBufOffset + lProc*bytesToCopy);
                    MEMCOPY_FUNC(src, dest, bytesToCopy);
                }
                sharedMemBufOffset += recvlens_m[host];
            }
        }  /* end if ( recvBuff ) */
#else
        /* the daemon process does not need this information */
        if( recvBuff ) {
            ssize_t sharedMemBufOffset;
            for (int host = 0; host < nhosts_m; host++) {
                for (int lProc=0 ; lProc < groupHostData_m[host].nGroupProcIDOnHost ; 
                     lProc++ ) {
                    int proc= groupHostData_m[host].groupProcIDOnHost[lProc];
                    sharedMemBufOffset=proc*bytesToCopy;
                    dest = (void *) ((char *) recvBuff + bytesAlreadyCopied+proc*bytesPerProc);
                    src = (void *) ((char *) sharedBuffer_m + sharedMemBufOffset);
                    MEMCOPY_FUNC(src, dest, bytesToCopy);
                    sharedMemBufOffset+=bytesToCopy;
                }
            }
        }  /* end if ( recvBuff ) */
#endif

        /* spin until all data has been received */
        localBarrier();

    }                           /* end stripeID loop */

    /* done with client code */
    goto ReturnCode;

ServerCode:

#if ENABLE_CT

    // mpirun should not need to be involved here.
    goto ReturnCode;
        
#endif

    aggregateData=(size_t *)ulm_malloc(bytesPerProc*totalNProcesses_m);
    dataArrivedFromHost=(int *)ulm_malloc(sizeof(int)*totalNProcesses_m);

    /* loop over data stripes */
    for (int stripeID = 0; stripeID < numStripes; stripeID++) {

        nHostsArrived=0;
        for( host=0 ; host < nhosts_m ; host++ ) {
            dataArrivedFromHost[host]=0;
        }
        ssize_t bytesAlreadyHandled=stripeID * perRankStripeSize;
        ssize_t leftToHandle = recvCount - bytesAlreadyHandled;
        ssize_t bytesToHandle = leftToHandle;
        if (bytesToHandle > perRankStripeSize)
            bytesToHandle = perRankStripeSize;

        /* loop until all data has arrived */
        while (nHostsArrived<nhosts_m){

            /* loop over all hosts */
            for( host=0 ; host < nhosts_m ; host++ ) {

                /* continue if already received data from this host */
                if( dataArrivedFromHost[host] )
                    continue;
                reset(adminMessage::RECEIVE);
                recvReturnCode=receive(host,&typeTag,&returnCode);

                /* no data to read  - continue */
                if( recvReturnCode == TIMEOUT ) {
                    continue;
                }

                /* error */
                if( recvReturnCode == ERROR ) {
                    ulm_err(("Error: from receive in adminMessage::allgather (%d)\n",
                             returnCode));
                    returnCode=ULM_ERROR;
                    return recvReturnCode;
                }

                /* data ok */
                if( recvReturnCode == OK ) {
                    /* make sure data is allgather data */
                    if( typeTag != ALLGATHER ) {
                        ulm_err(("Error: Unexpected data type in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }

                    /* check tag */
                    bReturnValue=unpack(&tmpTag,
                                        (adminMessage::packType)sizeof(long long), 1);
                    if( !bReturnValue ) {
                        ulm_err(("Error: from unpack in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }
                    if( tmpTag != tag ) {
                        ulm_err(("Error: Tag mismatch in adminMessage::allgather\n"
                                 "\t Expected %lld - Arrived %lld\n",
                                 tag, tmpTag));
                    }
                    /* unpack data */
                    offsetIntoAggregateData=0;
                    for(hst=0; hst < host ; hst++) {
                        offsetIntoAggregateData+=(groupHostData_m[hst].nGroupProcIDOnHost*
                                                  bytesToHandle);
                    }
                    dest=( (char *)aggregateData ) + offsetIntoAggregateData;
                    totalBytes=groupHostData_m[host].nGroupProcIDOnHost*bytesToHandle;
                    bReturnValue=unpack(dest,BYTE,totalBytes);
                    if( !bReturnValue ) {
                        ulm_err(("Error: from unpack in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }

                    /* update received data counter */
                    nHostsArrived++;
                }
            }
        }  /* end while (nHostsArrived<nhosts_m) loop */

        /* broadcast the data to the clients */
        totalBytes = bytesToHandle*totalNProcesses_m;
        bReturnValue = reset(adminMessage::SEND, totalBytes);
        if( !bReturnValue )
            return ULM_ERROR;

        bReturnValue = pack(aggregateData, BYTE, totalBytes);
        if( !bReturnValue )
            return ULM_ERROR;

        bReturnValue = broadcast(ALLGATHER, &returnCode);
        if( !bReturnValue ) {
            ulm_err(("Error: from broadcast in adminMessage::allgather (%d)\n",
                     returnCode));
            return ULM_ERROR;
        }

    } /* end loop over data stripes */

    ulm_free(aggregateData);
    ulm_free(dataArrivedFromHost);

ReturnCode:

    return returnCode;
}



int adminMessage::scatterv(int root, int tag, int *errorCode)
{
#if ENABLE_CT
    CTChannelStatus         status;
    bool                            success;
    int                                     rank;
        
    success = true;
    if ( (false == client_m) ||  (hostRank_m == root) )
    {
        //ASSERT: (false == client_m) => this is being called by mpirun
        if ( false == client_m )
            status = netconn_m->scatterv(CTMessage::kUser, (unsigned int *)scatterLen_m, (const char **)scatterHosts_m);
        else
            status = daemon_m->scatterv(CTMessage::kUser, (unsigned int *)scatterLen_m, (const char **)scatterHosts_m);
    }
    else
    {
        rank = root;
        // keep polling msg queue until we've received our scatter msg.
        // kludge to differentiate who is sending: mpirun or another node
        reset(RECEIVE);
        success = getMessageFromQueue(&rank, &tag, CTMessage::kScatterv, errorCode, -1);
    }
    return success;
#endif
    return 0;
}

void adminMessage::synchronize(int nMembers)
{
#if ENABLE_CT
    if ( client_m )
    {
        daemon_m->synchronize(nMembers);
    }
    else
    {
        netconn_m->synchronize(nMembers);
    }
#endif
}
        

/* this is a primtive implementation of a barrier - it can 
 *   optionally include the deamon process, if such exists
 */
void adminMessage::localBarrier()
{

    // increment "release" count
    barrierData_m->releaseCnt += barrierData_m->commSize;
        
    // increment fetch and op variable
    barrierData_m->lock->lock();
    *(barrierData_m->Counter) += 1;
    barrierData_m->lock->unlock();
    
    // spin until fetch and op variable == release count
    while (*(barrierData_m->Counter) < barrierData_m->releaseCnt)
        ;

    return;
}

/* get a tag - not thread safe.  This is assumed to be used by the
 * library in a non-threaded region
 */
long long adminMessage::get_base_tag(int num_requested) {
    long long ret_tag;
    if (num_requested < 1)
        return -1;
    ret_tag = collectiveTag_m;
    collectiveTag_m -= num_requested;
    return ret_tag; 
}

int adminMessage::setupCollectives(int myLocalRank, int myHostRank,
                                   int *map_global_rank_to_host, int daemonIsHostCommRoot, 
                                   ssize_t lenSMBuffer, void *sharedBufferPtr, 
                                   void *lockPtr, void *CounterPtr, int *sPtr)
{
    /* */
    int proc,host,count;
    int localCommSize = 0;

    /* set host comm-root */
    if( daemonIsHostCommRoot )
        hostCommRoot_m=DAEMON_PROC;
    else
        hostCommRoot_m=0;

    /* set localProcessRank_m */
    localProcessRank_m=myLocalRank;

#if ENABLE_CT
    if ( NULL == (recvlens_m = (int *)ulm_malloc(nhosts_m * sizeof(int))) )
        return ULM_ERROR;
#endif

    /* 
     * set up groupHostData_m 
     */

    /* allocate memory */
    if( !groupHostData_m ){
        groupHostData_m=(admin_host_info_t *)
            ulm_malloc(nhosts_m*sizeof(admin_host_info_t));
        for( host=0 ; host < nhosts_m ; host++ )
            groupHostData_m[host].groupProcIDOnHost=(int*)0;

    }

    /* loop over host count */
    for(host=0 ; host < nhosts_m ; host++ ) {
        count=0;
        /* figure out how many procs on given host */
        for(proc=0 ; proc < totalNProcesses_m ; proc++ ) {
            if( map_global_rank_to_host[proc]==host )
                count++;
        }
        /* set process count */
        groupHostData_m[host].nGroupProcIDOnHost=
            count;

        if( host == myHostRank )
            localCommSize=count;

        /* allocate memory for list of procs */
        if( groupHostData_m[host].groupProcIDOnHost )
            ulm_free(groupHostData_m[host].groupProcIDOnHost);
        groupHostData_m[host].groupProcIDOnHost= (int *)
            ulm_malloc(sizeof(int)*count);

        /* fill in list of procs */
        count=0;
        for(proc=0 ; proc < totalNProcesses_m ; proc++ ) {
            if( map_global_rank_to_host[proc]==host ){
                groupHostData_m[host].groupProcIDOnHost[count]=
                    proc;
                count++;
            }
        }
    } /* end host loop */

    /* 
     * initialize barrier structure 
     */

    barrierData_m=(swBarrierData *)ulm_malloc(sizeof(swBarrierData));
    if ( daemonIsHostCommRoot )
        localCommSize++;
    barrierData_m->commSize=localCommSize;
    barrierData_m->releaseCnt=0;

    /* finish setting barrier structures */
    barrierData_m->lock = (Locks *)lockPtr;
    barrierData_m->lock->init();

    /* set barrier Counter */
    barrierData_m->Counter = (volatile long long *) CounterPtr;
    *(barrierData_m->Counter) = 0;

    /* set collectiveTag_m */
    collectiveTag_m = -1;
    syncFlag_m = sPtr;

    /* set shared memory payload buffer */
    sharedBuffer_m = (void *)sharedBufferPtr;
    lenSharedMemoryBuffer_m = lenSMBuffer;

    return ULM_SUCCESS;
}

void *server_connect_accept(void *arg) 
{
    adminMessage *server = (adminMessage *)arg;
    sigset_t signals;
#ifdef __linux__
    socklen_t addrlen;
#else
    int addrlen;
#endif
    struct sockaddr_in addr;
    fd_set fds;
    struct timeval t;

    t.tv_sec = 0;
    t.tv_usec = 10000;

    /* disable SIGALRM for this thread */
    sigemptyset(&signals);
    sigaddset(&signals, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &signals, (sigset_t *)NULL);

    /* enable deferred cancel mode for this thread */
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, (int *)NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, (int *)NULL);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(server->serverSocket_m, &fds);
        if (select(server->serverSocket_m + 1, &fds, (fd_set *)NULL, (fd_set *)NULL, &t) <= 0)  {
            pthread_testcancel();
            continue;
        }

        addrlen = sizeof(addr);
        int sockfd = accept(server->serverSocket_m, (struct sockaddr *)&addr, &addrlen);
        if (sockfd < 0) {
            continue;
        }

        if (sockfd >= adminMessage::MAXSOCKETS) {
            ulm_err(("server_connect_accept(%p) client socket fd, %d, greater than "
                     "allowed MAXSOCKETS, %d\n", server, sockfd, adminMessage::MAXSOCKETS));
            close(sockfd);
            pthread_exit((void *)0);
        }

        server->socketsToProcess_m[sockfd] = true;
        pthread_testcancel();
    }
    return NULL;
}

/* (server) initialize socket and wait for all connections from clients
 * timeout (in): time in seconds to wait for all connections from clients
 * returns: true if successful, false if unsuccessful (requiring program exit)
 */
bool adminMessage::serverConnect(int* procList, HostName_t* hostList,
                                 int numHosts, int timeout) 
{
    int np = 0, nh=0, tag, hostrank, nprocesses, recvAuthData[3], ok;
    int oldLCS = largestClientSocket_m,cnt;
    ulm_iovec_t iovecs[5];
    int size, *hostsAssigned = 0;
    pid_t daemonPid;
    pthread_t sca_thread;

#if ENABLE_BPROC == 0
    int assignNewId;
#ifdef __linux__
    socklen_t addrlen;
#else
    int addrlen;
#endif
    struct sockaddr_in addr;
#else
    int use_random_mapping = 0; 
#endif

#if ENABLE_CT
    return collectDaemonInfo(procList, hostList, numHosts, timeout);
#endif

    /* initialization "stuff" */
    if( numHosts > 0 ) { 
        hostsAssigned=(int *)ulm_malloc(numHosts*sizeof(int));
        if( !hostsAssigned ) {
            ulm_err(("Error: Can't allocate memory for hostsAssigned list\n"));
            return false;
        }
        for(int i=0 ; i < numHosts ; i++ )
            hostsAssigned[i]=0;
    }

    if (timeout > 0) {
        if (sigaction(SIGALRM, &newSignals, &oldSignals) < 0) {
            return false;
        }
        if (setjmp(savedEnv) != 0) {
            struct sockaddr_in addr;
            struct hostent *h;
            ulm_err(("adminMessage::serverConnect timeout %d exceeded --"
                     " %d client sockets account for %d processes!\n",
                     timeout, clientSocketCount(), clientProcessCount()));
	    ulm_err(("\n\nmpirun was unable to start you application.\n"
        	"This may be caused by several things \n"
             	"- the application may not exist on the remote node\n"
                 "- the application may not be executable on the remote node\n"
                 "- the loader may not be able to find the dynamic libraries \n"
                 "  needed to run the application.\n"
                 "- check to see that your remote LD_LIBRARY_PATH points to the \n"
                 "  correct MPI library, and to any other dynamic libraries your \n"
                 "  application needs. \n"
                 "- make sure that the mpirun you are using goes with the MPI \n"
                 "  library you are trying to use.\n\n"));
		 
            for (int i = 0; i < largestClientSocket_m; i++) {
                if (clientSocketActive_m[i]) {
#ifdef __linux__
                    socklen_t addrlen = sizeof(struct sockaddr_in);
#else
                    int addrlen = sizeof(struct sockaddr_in);
#endif
                    int result = getpeername(i, (struct sockaddr *)&addr, &addrlen);
                    if (result == 0) {
                        h = gethostbyaddr((char *)(&addr.sin_addr.s_addr), 4, AF_INET);
                        ulm_err(("\tfd %d peer info: IP %s process count %d PID %ld\n", i,
                                 h ? h->h_name : inet_ntoa(addr.sin_addr), processCount[i], 
                                 (long)daemonPIDs_m[i]));
                    }
                }
            }
            return false;
        }
        alarm(timeout);
    }

    // wait for all clients to connect

    // initialize processCount and daemonPIDs
    for(cnt=0 ; cnt < MAXSOCKETS ; cnt++){
        processCount[cnt]=(int)0;
        daemonPIDs_m[cnt]=(size_t)0;
        socketsToProcess_m[cnt] = false;
    }

    // spawn thread to do accept processing only
    if (pthread_create(&sca_thread, (pthread_attr_t *)NULL, server_connect_accept, (void *)this) != 0) {
        ulm_err(("Error: can't create serverConnect() accept thread!\n"));
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
        }
        return false;
    }

    while (np != totalNProcesses_m) {
        int sockfd = -1;
        int offset = (largestClientSocket_m >= 0) ? largestClientSocket_m : 0;

        if (cancelConnect_m) {
            if (timeout > 0) {
             alarm(0);
             sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
            }
            pthread_cancel(sca_thread);
            cancelConnect_m = false;
            return false;
        }

        for (cnt = 0; cnt < MAXSOCKETS; cnt++) {
            if (socketsToProcess_m[(offset + cnt) % MAXSOCKETS]) {
                sockfd = (offset + cnt) % MAXSOCKETS;
                socketsToProcess_m[sockfd] = false;
                break;
            }
        }
        if (sockfd < 0) {
            continue;
        }

        // now do the authorization handshake...receive info
        iovecs[0].iov_base = &tag;
        iovecs[0].iov_len = (ssize_t)sizeof(int);
        iovecs[1].iov_base = recvAuthData;
        iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
        iovecs[2].iov_base = &hostrank;
        iovecs[2].iov_len = (ssize_t)sizeof(int);
        iovecs[3].iov_base = &nprocesses;
        iovecs[3].iov_len = (ssize_t)sizeof(int);
        iovecs[4].iov_base = &daemonPid;
        iovecs[4].iov_len = (ssize_t)sizeof(pid_t);
        if ((size=ulm_readv(sockfd, iovecs, 5)) != 
            6*sizeof(int)+sizeof(pid_t)) {
            ulm_err(("adminMessage::serverConnect read from client socket failed!\n"));
            ulm_err(("Error: received %d expected %d\n",size,6*sizeof(int)+sizeof(pid_t)));
            close(sockfd);
            continue;
        }

#if ENABLE_BPROC
        /* BPROC node id from remote bproc_currnode() call must be translated */
        int hostrank_orig=hostrank;
        hostrank = nodeIDToHostRank(numHosts, hostList, hostrank);
        
        
        // first time, specify if we are using a random host mapping, or actual (default):
        if (nh==0 && hostrank==numHosts) use_random_mapping=1;
        
        if (use_random_mapping)
            ulm_err(("Using random host mapping: nh=%d client sent nodeid=%d\n",nh,hostrank_orig));  

        // in random host map mode, if any host returns valid hostrank, fail:
        // in actual host map mode, if any host returns invalid hostrank, fail:
        if ((hostrank==numHosts && use_random_mapping==0) 
          ||  (hostrank!=numHosts && use_random_mapping==1)) {
            ulm_err(("Error: adminMessage::serverConnect nodeIDToHostRank failed (sockfd = %d)!\n", sockfd));
            if (timeout > 0) {
                alarm(0);
                sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
            }
            pthread_cancel(sca_thread);
           return false;
        }

        if (use_random_mapping==1) {
          hostrank=nh;  
        }
#else
        // set hostrank
        if( hostrank == UNKNOWN_HOST_ID ) {
            assignNewId=1;
            addrlen = sizeof(addr);
            getpeername(sockfd, (struct sockaddr *)&addr, &addrlen);
            hostrank=socketToHostRank(numHosts,hostList,&addr,
                                    assignNewId,hostsAssigned);
            if( hostrank == UNKNOWN_HOST_ID ){
                ulm_err(("Error: adminMessage::serverConnect UNKNOWN_HOST_ID (sockfd = %d)\n", sockfd));
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
                }
                pthread_cancel(sca_thread);
                return false;
            }
        }
#endif

        // cache process count
        if(nprocesses != UNKNOWN_N_PROCS )
        {
            processCount[hostrank]=nprocesses;
        }
        else {
            if( hostrank == UNKNOWN_HOST_ID ){
                ulm_err(("Error: adminMessage::serverConnect UNKNOWN_HOST_ID (sockfd = %d)\n", sockfd));
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
                }
                pthread_cancel(sca_thread);
                return false;
            }
        }

        // cache process count
        if(nprocesses != UNKNOWN_N_PROCS )
        {
            processCount[hostrank]=nprocesses;
        }
        else {
            processCount[hostrank]=procList[hostrank];
            nprocesses=procList[hostrank];
        }

        // client ok until proven guilty...
        ok = 1;
                        
        // check the message tag
        if (tag != INITMSG) {
            ulm_err(("adminMessage::serverConnect client socket received tag that"
                     " is not INITMSG (tag = %d)\n", tag));
            ok = 0;
        }

        // check the authorization data
        for (int i = 0; i < 3; i++) {
            if (recvAuthData[i] != authData_m[i]) {
                ulm_err(("adminMessage::serverConnect client socket authorization data bad!\n"));
                ok = 0;
                break;
            }
        }

        // check the global host rank to make sure it is unique
        for (int i = 0; i < largestClientSocket_m; i++) {
            if (clientSocketActive_m[i] && (ranks_m[i] == hostrank)) {
                ulm_err(("adminMessage::serverConnect duplicate host rank %d\n", hostrank));
                ok = 0;
                break;
            }
        }

        // store the information about this socket
        if (ok == 1) {
            np += nprocesses;
            ++nh;
            ranks_m[sockfd] = hostrank;
            clientSocketActive_m[sockfd] = true;
            oldLCS = largestClientSocket_m;
            if (sockfd > largestClientSocket_m)
                largestClientSocket_m = sockfd;
        }


        // cache daemon PID
        daemonPIDs_m[hostrank]=daemonPid;
           
        // send reply INITOK message
        tag = INITOK;
        iovecs[0].iov_base = &tag;
        iovecs[0].iov_len = (ssize_t)sizeof(int);
        iovecs[1].iov_base = authData_m;
        iovecs[1].iov_len = (ssize_t)(3 * sizeof(int));
        iovecs[2].iov_base = &ok;
        iovecs[2].iov_len = (ssize_t)sizeof(int);
        if (ulm_writev(sockfd, iovecs, 3) != 5*sizeof(int)) {
            ulm_err(("adminMessage::serverConnect write to client socket failed!\n"));
            close(sockfd);
            if (ok == 1) {
                np -= nprocesses;
                clientSocketActive_m[sockfd] = false;
                largestClientSocket_m = oldLCS;
            }
            continue;
        }
    }  /* end while (np != totalNProcesses_m) */

    if (timeout > 0) {
        alarm(0);
        sigaction(SIGALRM, &oldSignals, (struct sigaction *)NULL);
    }

    nhosts_m = clientSocketCount();

    if( numHosts > 0 )
        ulm_free(hostsAssigned);

    pthread_cancel(sca_thread);
    return true;
}

/* send data from send buffer (or receive buffer if useRecvBuffer is true) to all destinations
 * tag (in): message tag value
 * errorCode (out): errorCode if false is returned
 * returns: true if successful, false if unsuccessful (errorCode is then set)
 */
bool adminMessage::broadcast(int tag, int *errorCode) {
    bool returnValue = true;
    ulm_iovec_t iovecs[2];
    int bytesToWrite, bytesWritten;

#if ENABLE_CT
    return broadcastMessage(tag, errorCode);
#endif

    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = sizeof(int);
    iovecs[1].iov_base = sendBuffer_m;
    iovecs[1].iov_len = sendOffset_m;
    bytesToWrite = sendOffset_m + sizeof(int);

    if (client_m) {
        if (socketToServer_m >= 0) {
            bytesWritten = ulm_writev(socketToServer_m, iovecs, (sendOffset_m) ? 2 : 1);
            if (bytesWritten != bytesToWrite) {
                *errorCode = errno;
                returnValue = false;
                return returnValue;
            }
        }
    }

    if (server_m) {
        for (int i = 0; i <= largestClientSocket_m; i++) {
            if (clientSocketActive_m[i]) {
                bytesWritten = ulm_writev(i, iovecs, (sendOffset_m) ? 2 : 1);
                if (bytesWritten != bytesToWrite) {
                    *errorCode = errno;
                    returnValue = false;
                    return returnValue;
                }
            }
        }
    }

    return returnValue;
}

        
bool adminMessage::broadcastMessage(int tag, int *errorCode)
{
#if ENABLE_CT    
    CTMessage                       *msg;
    CTChannelStatus         status;
    unsigned int            ctrl;
    bool                            success;
        
    success = true;
    // move the buffer over so that we can insert the tag.
    if ( (unsigned int)sendBufferSize_m < (sendOffset_m + sizeof(tag)) )
        sendBuffer_m = (unsigned char *)realloc(sendBuffer_m, sendOffset_m + sizeof(tag));
                
    memmove(sendBuffer_m + sizeof(tag), sendBuffer_m, sendOffset_m);
    memcpy(sendBuffer_m, &tag, sizeof(tag));
    msg = new CTMessage(CTMessage::kUser, (const char *)sendBuffer_m, sendOffset_m + sizeof(tag));
        
    if ( msg )
    {
        if (client_m)
        {
            status = daemon_m->broadcast(msg);
        }
        else
        {
            ctrl = HypercubeNode::initialControlData(nhosts_m);
            status = netconn_m->broadcast(msg, sizeof(ctrl), (char *)&ctrl);
        }
                
        if ( kCTChannelOK != status )
        {
            ulm_err( ("Error: while broadcasting msg. status = %d.\n", status) );
            success = false;
        }
                 
        msg->release();
    }
    else
    {
        ulm_err( ("Error: Unable to create broadcast msg.\n") );
        return false;
    }
    return success;
#endif
    return false;
}


bool adminMessage::getMessageFromQueue(int *rank, int *tag, int routingType, int *errorCode, int timeout)
{
#if ENABLE_CT
    // Keep checking msg queue until we get the msg or until timed out
    // translate rank to node label. Places msg content in recvBuffer.
    struct timeval          endtime, curtime;
    qitem_t                         *item;
    bool                            didtimeout, done;
    mkeys_t                         keys;
    CTMessage                       *msg;
        
    // if tag == -1 then grab msg with any tag
    // if label == -1 then grab msg with any source
    keys.label = *rank;
    keys.tag = *tag;
    keys.rtype = routingType;
        
    gettimeofday(&endtime, NULL);
    endtime.tv_sec += timeout;
        
    didtimeout = false;
    done = false;
    do
    {
        qlock_m.lock();
        item = (qitem_t *)remove_item(msgQueue_m, &msgQueue_m, match_msg, &keys);
        qlock_m.unlock();
        if ( item )
        {
            // copy data to buffer; skip tag when copying
            msg = item->msg;
            *rank = msg->sourceNode();
            if ( recvBufferSize_m < msg->dataLength() )
            {
                recvBuffer_m = (unsigned char *)realloc(recvBuffer_m, msg->dataLength());
                recvBufferSize_m = msg->dataLength();
                if ( NULL == recvBuffer_m )
                {
                    ulm_err(("Unable to alloc memory for recv buffer.\n"));
                    free_qitem(item);
                    return false;
                }                
            }
                        
            // copy tag
            *tag = *((int *)msg->data());
            // skip tag
            recvBufferBytes_m = msg->dataLength() - sizeof(int);
            memcpy(recvBuffer_m, msg->data() + sizeof(int), recvBufferBytes_m);
            recvMessageBytes_m = recvBufferBytes_m;
            free_qitem(item);
                        
            done = true;
        }
        else
        {
            usleep(100);
            if ( timeout >= 0 )
            {
                gettimeofday(&curtime, NULL);
                if ( curtime.tv_sec > endtime.tv_sec )
                {
                    didtimeout = true;
                }                       
            }
        }
    } while ( !didtimeout && !done );
        
    if ( didtimeout )
    {
        *errorCode = TIMEOUT;
        return false;
    }
    else
        return true;
#endif
    return false;
}



bool adminMessage::receiveMessage(int *rank, int *tag, int *errorCode, int timeout)
{
    CTChannelStatus status;
    CTMessage               *msg;
    bool                    returnValue = true, getmsg = true;
    int                             to;

    reset(RECEIVE);
    if ( client_m )
    {
        return getMessageFromQueue(rank, tag, 0, errorCode, timeout);
    }
    else
    {
        // timeout < 0 => poll for msgs.
        to = netconn_m->channel()->timeout();
        if ( timeout >= 0 )
        {
            netconn_m->channel()->setTimeout(timeout);
        }
        else
        {
            if ( false == netconn_m->channel()->isReadable() )
            {
                getmsg = false;
                *errorCode = NOERROR;
                returnValue = false;
            }
        }
                
        if ( getmsg )
        {
            status = netconn_m->channel()->getMessage(&msg);
            if ( kCTChannelOK == status )
            {
                // get tag
                memcpy(tag, msg->data(), sizeof(int));
                                
                // get data
                recvBufferBytes_m = msg->dataLength() - sizeof(int);	// do not include tag size
                memcpy(recvBuffer_m, msg->data() + sizeof(int), recvBufferBytes_m);
                recvMessageBytes_m = recvBufferBytes_m;
        
                *rank = msg->sourceNode();
                                
                msg->release();
            }
            else if ( kCTChannelClosed == status )
            {
                returnValue = false;
                *errorCode = NOERROR;
            }
            else
            {
                returnValue = false;
                *errorCode = ( kCTChannelTimedOut == status ) ? TIMEOUT : ERROR;
            }
        }
                
        netconn_m->channel()->setTimeout(to);
    }

    return returnValue;
}
        



bool adminMessage::sendMessage(int rank, int tag, unsigned int channelID, int *errorCode) 
{
    CTMessage                       *msg;
    CTChannelStatus         status;
    unsigned int            ctrl;
    bool                            success;
        
    success = true;
    // move the buffer over so that we can insert the tag.
    if ( (unsigned int)sendBufferSize_m < (sendOffset_m + sizeof(tag)) )
        sendBuffer_m = (unsigned char *)realloc(sendBuffer_m, sendOffset_m + sizeof(tag));
                
    memmove(sendBuffer_m + sizeof(tag), sendBuffer_m, sendOffset_m);
    memcpy(sendBuffer_m, &tag, sizeof(tag));
    msg = new CTMessage(CTMessage::kUser, (const char *)sendBuffer_m, sendOffset_m + sizeof(tag));
        
    if ( msg )
    {
        msg->setDestination((unsigned int)rank);
        if (client_m)
        {
            msg->setDestinationClientID(channelID);
            status = daemon_m->sendMessage(msg);
        }
        else
        {
            msg->setSendingClientID(channelID);
            ctrl = HypercubeNode::initialControlData(nhosts_m);
            status = netconn_m->sendMessage(msg, sizeof(ctrl), (char *)&ctrl);
        }
                
        if ( kCTChannelOK != status )
        {
            ulm_err( ("Error: while sending msg. status = %d.\n", status) );
            success = false;
        }
                 
        msg->release();
    }
    else
    {
        ulm_err( ("Error: Unable to create msg.\n") );
        return false;
    }
    return success;
}

bool adminMessage::send(int rank, int tag, int *errorCode) {
    bool returnValue = true;
    ulm_iovec_t iovecs[2];
    int sockfd = (rank == -1) ? socketToServer_m : clientRank2FD(rank);

    if (sockfd < 0) {
        *errorCode = ULM_ERR_RANK;
        returnValue = false;
        return returnValue;
    }

    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = sizeof(int);
    iovecs[1].iov_base = sendBuffer_m;
    iovecs[1].iov_len = sendOffset_m;
    if (ulm_writev(sockfd, iovecs, (sendOffset_m) ? 2 : 1) != (int)(sendOffset_m + sizeof(int))) {
        *errorCode = errno;
        returnValue = false;
        return returnValue;
    }

    return returnValue;
}

bool adminMessage::reset(direction dir, int size)
{
    bool returnValue = true;

    if ( SCATTERV == dir )
    {
        // reserve first item in send array for the sending tag.
        sendOffset_m = 0;
        if (size && (size > sendBufferSize_m))
        {
            unsigned char *tmp = (unsigned char *)realloc(sendBuffer_m, size);
            if (tmp)
            {
                sendBufferSize_m = size;
                sendBuffer_m = tmp;
            }
            else {
                returnValue = false;
            }
        }
        curHost_m = 0;
    }
        
    if ((dir == SEND) || (dir == BOTH)) {
        sendOffset_m = 0;
        if (size && (size > sendBufferSize_m)) {
            unsigned char *tmp = (unsigned char *)realloc(sendBuffer_m, size);
            if (tmp) {
                sendBufferSize_m = size;
                sendBuffer_m = tmp;
            }
            else {
                returnValue = false;
            }
        }
    }

    if ((dir == RECEIVE) || (dir == BOTH)) {
        recvOffset_m = 0;
        recvBufferBytes_m = 0;
        recvMessageBytes_m = 0;
        if (size && (size > recvBufferSize_m)) {
            unsigned char *tmp = (unsigned char *)realloc(recvBuffer_m, size);
            if (tmp) {
                recvBufferSize_m = size;
                recvBuffer_m = tmp;
            }
            else {
                returnValue = false;
            }
        }
    }
    return returnValue;
}


void adminMessage::setScattervHost() 
{
    /*
      POST: scatterHosts_m[curHost_m - 1] = ptr in sendBuffer_m that contains scatter data
      for host curHost_m - 1.
    */
    if ( 0 == curHost_m )
    {
        scatterHosts_m[curHost_m] = sendBuffer_m;
        scatterLen_m[curHost_m] = sendOffset_m;
    }
    else
    {
        scatterHosts_m[curHost_m] = scatterHosts_m[curHost_m - 1] + scatterLen_m[curHost_m - 1];
        scatterLen_m[curHost_m] = sendBuffer_m + sendOffset_m - scatterHosts_m[curHost_m];
    }
    curHost_m++;
}


bool adminMessage::pack(void *data, packType type, int count, packFunction pf, packSizeFunction psf) 
{
    bool returnValue = true;
    void *dst = (void *)(sendBuffer_m + sendOffset_m);

    if (count <= 0)
        return returnValue;

    if (!data) {
        ulm_err(("adminMessage::pack data is NULL pointer (count = %d)\n", count));
        returnValue = false;
        return returnValue;
    }

    switch (type) {
    case BYTE:
    {
        if ((int)(sendOffset_m + count*sizeof(unsigned char)) > sendBufferSize_m) {
            ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n",
                     type, sendOffset_m + count*sizeof(unsigned char)));
            returnValue = false;
        }
        else {
            MEMCOPY_FUNC(data, dst, sizeof(unsigned char)*count);
            sendOffset_m += count*sizeof(unsigned char);
        }
    }
    break;
    case SHORT:
    {
        if ((int)(sendOffset_m + count*sizeof(unsigned short)) > sendBufferSize_m) {
            ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n",
                     type, sendOffset_m + count*sizeof(unsigned short)));
            returnValue = false;
        }
        else {
            MEMCOPY_FUNC(data, dst, sizeof(unsigned short)*count);
            sendOffset_m += count*sizeof(unsigned short);
        }
    }
    break;
    case INTEGER:
    {
        if ((int)(sendOffset_m + count*sizeof(unsigned int)) > sendBufferSize_m) {
            ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n",
                     type, sendOffset_m + count*sizeof(unsigned int)));
            returnValue = false;
        }
        else {
            MEMCOPY_FUNC(data, dst, sizeof(unsigned int)*count);
            sendOffset_m += count*sizeof(unsigned int);
        }
    }
    break;
    case LONGLONG:
    {
        if ((int)(sendOffset_m + count*sizeof(unsigned long long)) > sendBufferSize_m) {
            ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n",
                     type, sendOffset_m + count*sizeof(unsigned long long)));
            returnValue = false;
        }
        else {
            MEMCOPY_FUNC(data, dst, sizeof(unsigned long long)*count);
            sendOffset_m += count*sizeof(unsigned long long);
        }
    }
    break;
    case USERDEFINED:
    {
        if (!psf || !pf) {
            ulm_err(("adminMessage:pack pack size function (psf = %p) and pack function (pf = %p)\n",
                     psf, pf));
            returnValue = false;
            return returnValue;
        }
        int neededBytes = (*psf)(count);
        if ((sendOffset_m + neededBytes) > sendBufferSize_m) {
            ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n",
                     type, sendOffset_m + neededBytes));
            returnValue = false;
        }
        else {
            (*pf)(data, dst, count);
            sendOffset_m += neededBytes;
        }
    }
    break;
    default:
        ulm_err(("adminMessage::pack unknown packType %d\n", type));
        returnValue = false;
        break;
    }

    return returnValue;
}

adminMessage::recvResult adminMessage::nextTag(int *tag)
{
    if ( true == unpackMessage(tag, INTEGER, 1) )
        return OK;
    else
        return ERROR;
}
        

bool adminMessage::unpackMessage(void *data, packType type, int count, int timeout, unpackFunction upf,
                                 unpackSizeFunction upsf)
{
    bool returnValue = true;
    void *src = (void *)(recvBuffer_m);

    if (count <= 0)
        return returnValue;

    if (!data) {
        ulm_err(("adminMessage::unpack data is NULL pointer (count = %d)\n", count));
        returnValue = false;
        return returnValue;
    }

    // repack the receive buffer, if needed
    if (recvOffset_m && (recvBufferBytes_m > recvOffset_m)) {
        memmove(recvBuffer_m, (void *)(recvBuffer_m + recvOffset_m), recvBufferBytes_m - recvOffset_m);
        recvBufferBytes_m -= recvOffset_m;
        recvOffset_m = 0;
    }

    switch (type) {
    case BYTE:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned char)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned char)));
            returnValue = false;
        }
        else
        {
            MEMCOPY_FUNC(src, data, sizeof(unsigned char)*count);
            recvOffset_m += count*sizeof(unsigned char);
            if (recvOffset_m == recvBufferBytes_m)
            {
                recvOffset_m = recvBufferBytes_m = 0;
            }
        }
    }
    break;
    case SHORT:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned short)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned short)));
            returnValue = false;
        }
        else 
        {
            MEMCOPY_FUNC(src, data, sizeof(unsigned short)*count);
            recvOffset_m += count*sizeof(unsigned short);
            if (recvOffset_m == recvBufferBytes_m)
            {
                recvOffset_m = recvBufferBytes_m = 0;
            }
        }
    }
    break;
    case INTEGER:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned int)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned int)));
            returnValue = false;
        }
        else 
        {
            MEMCOPY_FUNC(src, data, sizeof(unsigned int)*count);
            recvOffset_m += count*sizeof(unsigned int);
            if (recvOffset_m == recvBufferBytes_m)
            {
                recvOffset_m = recvBufferBytes_m = 0;
            }
        }
    }
    break;
    case LONGLONG:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned long long)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned long long)));
            returnValue = false;
        }
        else 
        {
            MEMCOPY_FUNC(src, data, sizeof(unsigned long long)*count);
            recvOffset_m += count*sizeof(unsigned long long);
            if (recvOffset_m == recvBufferBytes_m)
            {
                recvOffset_m = recvBufferBytes_m = 0;
            }
        }
    }
    break;
    case USERDEFINED:
    {
        if (!upsf || !upf) {
            ulm_err(("adminMessage:unpack unpack size function (upsf = %p) and unpack function (upf = %p)\n",
                     upsf, upf));
            returnValue = false;
            return returnValue;
        }
        int neededBytes = (*upsf)(count);
        if ((recvOffset_m + neededBytes) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + neededBytes));
            returnValue = false;
        }
        else
        {
            (*upf)(recvBuffer_m, data, count);
            recvOffset_m += neededBytes;
            if (recvOffset_m == recvBufferBytes_m)
            {
                recvOffset_m = recvBufferBytes_m = 0;
            }
        }
    }
    break;
    default:
        ulm_err(("adminMessage::unpack unknown packType %d\n", type));
        returnValue = false;
        break;
    }

    return returnValue;
}




bool adminMessage::unpack(void *data, packType type, int count, int timeout, unpackFunction upf,
                          unpackSizeFunction upsf) {
    bool returnValue = true, gotData = true;
    void *src = (void *)(recvBuffer_m);

#if ENABLE_CT
    return unpackMessage(data, type, count, timeout, upf, upsf);
#endif

    if (count <= 0)
        return returnValue;

    if (!data) {
        ulm_err(("adminMessage::unpack data is NULL pointer (count = %d)\n", count));
        returnValue = false;
        return returnValue;
    }

    if (lastRecvSocket_m < 0) {
        ulm_err(("adminMessage::unpack receive method must be called first!\n"));
        returnValue = false;
        return returnValue;
    }

    // repack the receive buffer, if needed
    if (recvOffset_m && (recvBufferBytes_m > recvOffset_m)) {
        memmove(recvBuffer_m, (void *)(recvBuffer_m + recvOffset_m), recvBufferBytes_m - recvOffset_m);
        recvBufferBytes_m -= recvOffset_m;
        recvOffset_m = 0;
    }

    switch (type) {
    case BYTE:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned char)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned char)));
            returnValue = false;
        }
        else {
            if (recvBufferBytes_m < (int)(count*sizeof(unsigned char))) {
                gotData = getRecvBytes(count*sizeof(unsigned char) - recvBufferBytes_m, timeout);
            }
            if (gotData) {
                MEMCOPY_FUNC(src, data, sizeof(unsigned char)*count);
                recvOffset_m += count*sizeof(unsigned char);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
            else {
                ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d "
                         "bytes, timeout %d seconds)\n", count*sizeof(unsigned char), recvBufferBytes_m,
                         timeout));
                returnValue = false;
            }
        }
    }
    break;
    case SHORT:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned short)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned short)));
            returnValue = false;
        }
        else {
            if (recvBufferBytes_m < (int)(count*sizeof(unsigned short))) {
                gotData = getRecvBytes(count*sizeof(unsigned short) - recvBufferBytes_m, timeout);
            }
            if (gotData) {
                MEMCOPY_FUNC(src, data, sizeof(unsigned short)*count);
                recvOffset_m += count*sizeof(unsigned short);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
            else {
                ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d "
                         "bytes, timeout %d seconds)\n", count*sizeof(unsigned short), recvBufferBytes_m,
                         timeout));
                returnValue = false;
            }
        }
    }
    break;
    case INTEGER:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned int)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned int)));
            returnValue = false;
        }
        else {
            if (recvBufferBytes_m < (int)(count*sizeof(unsigned int))) {
                gotData = getRecvBytes(count*sizeof(unsigned int) - recvBufferBytes_m, timeout);
            }
            if (gotData) {
                MEMCOPY_FUNC(src, data, sizeof(unsigned int)*count);
                recvOffset_m += count*sizeof(unsigned int);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
            else {
                ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d "
                         "bytes, timeout %d seconds)\n", count*sizeof(unsigned int), recvBufferBytes_m,
                         timeout));
                returnValue = false;
            }
        }
    }
    break;
    case LONGLONG:
    {
        if ((int)(recvOffset_m + count*sizeof(unsigned long long)) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + count*sizeof(unsigned long long)));
            returnValue = false;
        }
        else {
            if (recvBufferBytes_m < (int)(count*sizeof(unsigned long long))) {
                gotData = getRecvBytes(count*sizeof(unsigned long long) - recvBufferBytes_m, timeout);
            }
            if (gotData) {
                MEMCOPY_FUNC(src, data, sizeof(unsigned long long)*count);
                recvOffset_m += count*sizeof(unsigned long long);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
            else {
                ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d "
                         "bytes, timeout %d seconds)\n", count*sizeof(unsigned long long), recvBufferBytes_m,
                         timeout));
                returnValue = false;
            }
        }
    }
    break;
    case USERDEFINED:
    {
        if (!upsf || !upf) {
            ulm_err(("adminMessage:unpack unpack size function (upsf = %p) and unpack function (upf = %p)\n",
                     upsf, upf));
            returnValue = false;
            return returnValue;
        }
        int neededBytes = (*upsf)(count);
        if ((recvOffset_m + neededBytes) > recvBufferSize_m) {
            ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n",
                     type, recvOffset_m + neededBytes));
            returnValue = false;
        }
        else {
            if (recvBufferBytes_m < neededBytes) {
                gotData = getRecvBytes(neededBytes - recvBufferBytes_m, timeout);
            }
            if (gotData) {
                (*upf)(recvBuffer_m, data, count);
                recvOffset_m += neededBytes;
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
            else {
                ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d "
                         "bytes, timeout %d seconds)\n", neededBytes, recvBufferBytes_m,
                         timeout));
                returnValue = false;
            }
        }
    }
    break;
    default:
        ulm_err(("adminMessage::unpack unknown packType %d\n", type));
        returnValue = false;
        break;
    }

    return returnValue;
}



/* received data into receive buffer from any destination
 * rank (out): global host rank of source, -1 for server
 * tag (out): message tag value
 * errorCode (out): errorCode if false is returned
 * timeout (in): -1 no timeout, 0 poll, > 0 milliseconds to wait in select
 * returns: OK if received message and needs to be handled, HANDLED if
 * received and automatically handled through a callback, TIMEOUT if
 * nothing received during the timeout, or ERROR if there is an error
 * (errrorCode is then set)
 */
adminMessage::recvResult adminMessage::receiveFromAny(int *rank, int *tag, int *errorCode, int timeout) {
    recvResult returnValue = OK;
    int sockfd = -1, maxfd = 0,s;
    struct timeval t;
    ulm_fd_set_t fds;
    ulm_iovec_t iovecs;

    reset(RECEIVE);

    if (timeout >= 0) {
        int seconds = timeout / (int)1000;
        int microseconds = timeout * 1000;
        if (seconds) {
            microseconds = (timeout - (seconds * 1000)) * 1000;
        }
        t.tv_sec = seconds;
        t.tv_usec = microseconds;
    }

    bzero(&fds, sizeof(fds));
    if ((client_m) && (socketToServer_m >= 0)) {
        FD_SET(socketToServer_m, (fd_set *)&fds);
        maxfd = socketToServer_m;
    }
    if (server_m) {
        for (int i = 0; i <= largestClientSocket_m; i++) {
            if (clientSocketActive_m[i]) {
                FD_SET(i, (fd_set *)&fds);
                if (i > maxfd) {
                    maxfd = i;
                }
            }
        }
    }

    if ((s = select(maxfd+1, (fd_set *)&fds, (fd_set *)NULL, (fd_set *)NULL,
                    (timeout < 0) ? (struct timeval *)NULL : &t)) > 0) {
        iovecs.iov_base = tag;
        iovecs.iov_len = sizeof(int);
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, (fd_set *)&fds)) {
                sockfd = i;
                *rank = clientFD2Rank(i);
                break;
            }
        }
        if (sockfd < 0) {
            *errorCode = ULM_ERROR;
            returnValue = ERROR;
            return returnValue;
        }
        if (ulm_readv(sockfd, &iovecs, 1) != sizeof(int)) {
            *errorCode = errno;
            returnValue = ERROR;
            return returnValue;
        }
        lastRecvSocket_m = sockfd;
    }
    else {
        returnValue = (s == 0) ? TIMEOUT : ERROR;
        if (returnValue == ERROR)
            *errorCode = errno;
        return returnValue;
    }

    if (callbacks_m[*tag]) {
        bool callReturn = (*callbacks_m[*tag])(this, *rank, *tag);
        if (callReturn) {
            returnValue = HANDLED;
        }
        else {
            returnValue = ERROR;
            *errorCode = ULM_ERROR;
        }
    }

    return returnValue;
}
        
        



int adminMessage::allgatherv(void *sendbuf, void *recvbuf, ssize_t *bytesPerProc)
{
    return 0;
}

/* return true if the name or IP address in dot notation of the peer to be stored at dst, or false if there is an error */
bool adminMessage::peerName(int hostrank, char *dst, int bytes, bool useDottedIP) {

#if ENABLE_CT
    int     cnt;
    char    **list;
    bool    success = true;
        
    // this logic is only for mpirun
    dst[0] = '\0';
    if ( client_m )
        return false;

    //connectInfo_m contains host name for node label.
    cnt = _ulm_parse_string(&list, connectInfo_m[hostrank], 1, ";");
    if ( 2 == cnt )
    {
        strcpy(dst, list[0]);
    }
    free_double_carray(list, cnt);
    
    return success;
#endif

    int	len;
    char	*dotted;
    int sockfd = (hostrank == -1) ? socketToServer_m : clientRank2FD(hostrank);
    struct sockaddr_in addr;
    struct hostent *h;
#ifdef __linux__
    socklen_t addrlen = sizeof(struct sockaddr_in);
#else
    int addrlen = sizeof(struct sockaddr_in);
#endif
    if (sockfd < 0) {
        return false;
    }

    if (getpeername(sockfd, (struct sockaddr *)&addr, &addrlen) != 0) {
        return false;
    }

    if ( useDottedIP )
    {
        // return the peerName using the dotted IP address.
        dotted = inet_ntoa(addr.sin_addr);
        len = strlen(dotted);
        bzero(dst, bytes);
        if (len <= (ssize_t)(bytes - 1)) {
            strcpy(dst, dotted);
        }
        else
        {
            return false;
        }
    }
    else
    {
        h = gethostbyaddr((char *)(&addr.sin_addr.s_addr), 4, AF_INET);
        dst[0] = '\0';
        if (h != (struct hostent *)NULL) {
            if (strlen(h->h_name) <= (size_t)(bytes - 1)) {
                strcpy(dst, h->h_name);
            }
            else if (strlen(inet_ntoa(addr.sin_addr)) <= (size_t)(bytes - 1)) {
                strcpy(dst, inet_ntoa(addr.sin_addr));
            }
            else {
                return false;
            }
        }
        else {
            if (strlen(inet_ntoa(addr.sin_addr)) <= (size_t)(bytes - 1)) {
                strcpy(dst, inet_ntoa(addr.sin_addr));
            }
            else {
                return false;
            }
        }
    }		// if ( useDottedIP )
    
    return true;
}

int adminMessage::sendBufferSize() {return sendBufferSize_m;}

int adminMessage::receivedMessageSize() {return recvMessageBytes_m;}

int adminMessage::parentHostRank()
{
    int		parent;
    
    if ( client_m && daemon_m )
    {
        daemon_m->node()->parent(0, (unsigned int *)&parent);
        return parent;
    }
    else
        return -1;
}


int adminMessage::numberOfDaemonChildren()
{
    //NOTE: since we could have node failures, then this number could change
    unsigned int		*children, cnt;

    if ( client_m && daemon_m )
    {
        children = daemon_m->node()->children(0, &cnt);
        ulm_free2(children);

        return cnt;
    }
    else
        return 0;
}


unsigned short adminMessage::serverPort()
{
#if ENABLE_CT
    return ((CTTCPChannel *)svrChannel_m->channel())->port();
#endif
    return 0;
}

unsigned int adminMessage::channelID()
{
    //ASSERT: netconn_m has connected to node 0
#if ENABLE_CT
    if ( !client_m )
        return netconn_m->clientID();
    else
        return 0;
#endif
    return 0;
}

unsigned int adminMessage::nodeLabel()
{
#if ENABLE_CT
    if ( client_m )
        return daemon_m->node()->label();
    else
        return 0;
#endif
    return 0;
}

void adminMessage::setHostRank(int rank)
{
    hostRank_m = rank;
}

admin_host_info_t *adminMessage::groupHostData() {return groupHostData_m;}
void adminMessage::setGroupHostData(admin_host_info_t *groupHostData)
{
    if ( groupHostData_m )
    {
        ulm_free2(groupHostData_m->groupProcIDOnHost);
        ulm_free2(groupHostData_m);
    }
    groupHostData_m = groupHostData;
}
    
ssize_t adminMessage::lenSharedMemoryBuffer() {return lenSharedMemoryBuffer_m;}
void adminMessage::setLenSharedMemoryBuffer(ssize_t len) {lenSharedMemoryBuffer_m = len;}

int adminMessage::localProcessRank() {return localProcessRank_m;}
void adminMessage::setLocalProcessRank(int rank) {localProcessRank_m = rank;}
    
void adminMessage::setNumberOfHosts(int nhosts)
{
    nhosts_m = nhosts;
}

int adminMessage::totalNumberOfProcesses()
{
    return totalNProcesses_m;
}

void adminMessage::setTotalNumberOfProcesses(int nprocs)
{
    totalNProcesses_m = nprocs;
}
