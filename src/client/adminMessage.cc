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

#include <assert.h>
#include <netdb.h>
#include <sys/socket.h>

#include "client/adminMessage.h"
#include "collective/coll_fns.h"
#include "internal/mpi.h"
#include "mem/ULMMallocMacros.h"
#include "os/atomic.h"          /* for fetchNadd */
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/MemFunctions.h"
#include "util/misc.h"

#ifndef __linux__
typedef int socklen_t;
#endif

#define MAX_RETRY		100

#define TIMEOUT_HELP_STRING \
"\n" \
"LA-MPI was unable to start your application.  This may be because:\n" \
"- The application may not exist or not be executable on the remote node.\n" \
"- The loader may not be able to find the dynamic libraries needed to run\n" \
"  run the application.\n" \
"\n" \
"Please verify:\n" \
"- The existence and permissions of the remote application.\n" \
"- That your LD_LIBRARY_PATH on the remote node includes the directory\n" \
"  containing the correct version of the LA-MPI library, and similarly\n" \
"  for other dynamic libraries your application needs.\n" \
"- The mpirun executable is the same version as the LA-MPI library.\n" \
"\n"

/*
 * This routine scans through the list of hosts in hostList, and return
 * the index of the entry that matches nodeid
 */
int nodeIDToHostRank(int numHosts, HostName_t * hostList, int nodeid)
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
int socketToHostRank(int numHosts, HostName_t * hostList,
                     struct sockaddr_in *client, int assignNewId, int *hostsAssigned)
{
    int j;
    int RetVal;
    struct hostent *TmpHost;
    int hostIndex = adminMessage::UNKNOWN_HOST_ID;
    TmpHost = gethostbyaddr((char *) &(client->sin_addr.s_addr), 4, AF_INET);
    if (TmpHost == NULL) {
        ulm_err(("socketToHostRank: unable to resolve: %s\n", inet_ntoa(client->sin_addr)));
        return hostIndex;
    }
    /* find order in list */
    for (j = 0; j < numHosts; j++) {
        RetVal = strncmp(hostList[j], TmpHost->h_name, strlen(TmpHost->h_name));
        if (RetVal == 0) {
            /* if this host index already used - continue */
            if (assignNewId && (hostsAssigned[j]))
                continue;
            hostIndex = j;
            if (assignNewId)
                hostsAssigned[j] = 1;
            break;
        }
    }                           /* end j loop */

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
    returnValue = returnValue && server->pack(&nhosts, adminMessage::INTEGER, 1);
    returnValue = returnValue && server->send(rank, tag, &errorCode);
    return returnValue;
}


adminMessage::adminMessage()
{
    client_m = false;
    server_m = false;
    connectInfo_m = NULL;

    nhosts_m = 0;
    hostRank_m = -2;
    sendBufferSize_m = DEFAULTBUFFERSIZE;
    recvBufferSize_m = DEFAULTBUFFERSIZE;
    sendBuffer_m = (unsigned char *) ulm_malloc(sendBufferSize_m);
    recvBuffer_m = (unsigned char *) ulm_malloc(recvBufferSize_m);

    if (!sendBuffer_m || !recvBuffer_m) {
        ulm_exit(("adminMessage::adminMessage unable to allocate %d "
                  "bytes for send/receive buffers\n",
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
        callbacks_m[i] = (callbackFunction) 0;
    }

    /* all signals will be blocked while handleAlarm is running */
    newSignals.sa_handler = &handleAlarm;
    (void) sigfillset(&newSignals.sa_mask);
    newSignals.sa_flags = 0;

    /* initialize collective resources */
    groupHostData_m = (admin_host_info_t *) NULL;
    barrierData_m = (swBarrierData *) NULL;
    sharedBuffer_m = 0;
    lenSharedMemoryBuffer_m = 0;
    syncFlag_m = (int *) NULL;
    localProcessRank_m = -2;
    hostCommRoot_m = -2;
    collectiveTag_m = -1;

}


/* default destructor */
adminMessage::~adminMessage()
{
    /* free collective resources */
    if (groupHostData_m)
        ulm_free(groupHostData_m);

    free_double_carray(connectInfo_m, nhosts_m);

    terminate();
}


bool adminMessage::clientInitialize(int *authData, char *hostname, int port)
{
    client_m = true;
    // already null-terminated in last byte for sure...
    // hostname_m is the name of host containing mpirun.
    strncpy(hostname_m, hostname, MAXHOSTNAMESIZE - 1);
    // store port and authentication handshake data for later use
    port_m = port;
    for (int i = 0; i < 3; i++) {
        authData_m[i] = authData[i];
    }

    return true;
}


bool adminMessage::clientConnect(int nprocesses, int hostrank, int timeout)
{
    int sockbuf = 1, tag = INITMSG;
    int ok, recvAuthData[3];
    struct sockaddr_in server;
    ulm_iovec_t iovecs[5];
    pid_t myPID;
    struct hostent *serverHost;

    socketToServer_m = socket(AF_INET, SOCK_STREAM, 0);
    if (socketToServer_m < 0) {
        ulm_err(("adminMessage::clientConnect unable to open TCP/IP socket!\n"));
        return false;
    }
    setsockopt(socketToServer_m, IPPROTO_TCP, TCP_NODELAY, &sockbuf, sizeof(int));
    serverHost = gethostbyname(hostname_m);

#if ENABLE_BPROC
    if (serverHost == (struct hostent *) NULL) {
        /* can't find server's address via hostname_m --> use BPROC utilities to find the master */
        int size = sizeof(struct sockaddr);
        if (bproc_nodeaddr(BPROC_NODE_MASTER, (struct sockaddr *) &server, &size) != 0) {
            ulm_err(("adminMessage::clientConnect error returned from the bproc_nodeaddr call :: errno - %d\n", errno));
            return false;
        }
    } else {
        memcpy((char *) &server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
    }
#else
    if (serverHost == (struct hostent *) NULL) {
        ulm_err(("adminMessage::clientConnect gethostbyname(\"%s\") failed (h_errno = %d)!\n",
                 hostname_m, h_errno));
        return false;
    }
    memcpy((char *) &server.sin_addr, serverHost->h_addr_list[0], serverHost->h_length);
#endif
    server.sin_port = htons((unsigned short) port_m);
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
            ulm_err(("adminMessage::clientConnect %d second timeout to %s exceeded!\n", timeout,
                     hostname_m));
            return false;
        }
        alarm(timeout);
    }
    // attempt to connect
    while(1) {
        if (connect(socketToServer_m, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) < 0) {
            if ((ETIMEDOUT == errno) || (ECONNREFUSED == errno)) {
                usleep(10);
            } else {
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
                }
                ulm_err(("adminMessage::clientConnect connect to server %s TCP port %d failed!\n",
                         hostname_m, port_m));
                close(socketToServer_m);
                socketToServer_m = -1;
                return false;
            }
        } else {
            break;
        }
    }

    // now do the authorization handshake...send info
    myPID = getpid();
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t) sizeof(int);
    iovecs[1].iov_base = authData_m;
    iovecs[1].iov_len = (ssize_t) (3 * sizeof(int));
    iovecs[2].iov_base = &hostrank;
    iovecs[2].iov_len = (ssize_t) sizeof(int);
    iovecs[3].iov_base = &nprocesses;
    iovecs[3].iov_len = (ssize_t) sizeof(int);
    iovecs[4].iov_base = &myPID;
    iovecs[4].iov_len = (ssize_t) sizeof(pid_t);
    if (ulm_writev(socketToServer_m, iovecs, 5) != (6 * sizeof(int) + sizeof(pid_t))) {
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
        }
        ulm_err(("adminMessage::clientConnect write to server socket failed!\n"));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }
    // check for OK message
    iovecs[0].iov_base = &tag;
    iovecs[0].iov_len = (ssize_t) sizeof(int);
    iovecs[1].iov_base = recvAuthData;
    iovecs[1].iov_len = (ssize_t) (3 * sizeof(int));
    iovecs[2].iov_base = &ok;
    iovecs[2].iov_len = (ssize_t) sizeof(int);
    if (ulm_readv(socketToServer_m, iovecs, 3) != 5 * sizeof(int)) {
        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
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
            sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
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
                sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
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
            sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
        }
        ulm_err(("adminMessage::clientConnect server does not give us the go-ahead to continue!\n"));
        close(socketToServer_m);
        socketToServer_m = -1;
        return false;
    }

    if (timeout > 0) {
        alarm(0);
        sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
    }
    return true;
}


bool adminMessage::serverInitialize(int *authData, int nprocs, int *port)
{
    socklen_t namelen;
    int sockbuf = 1;
    struct sockaddr_in socketInfo;

    namelen = sizeof(socketInfo);
    server_m = true;
    totalNProcesses_m = nprocs;
    for (int i = 0; i < 3; i++) {
        authData_m[i] = authData[i];
    }

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
    if (bind(serverSocket_m, (struct sockaddr *) &socketInfo, sizeof(struct sockaddr_in)) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to bind server TCP/IP socket!\n"));
        return false;
    }

    if (getsockname(serverSocket_m, (struct sockaddr *) &socketInfo, &namelen) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to get server socket information!\n"));
        return false;
    }
    *port = (int) ntohs(socketInfo.sin_port);
    if (listen(serverSocket_m, SOMAXCONN) < 0) {
        ulm_err(("adminMessage::serverInitialize unable to listen on server socket!\n"));
        return false;
    }

    return true;
}


/*
 * this primitive implementation of allgather handles only contiguous
 * data.  In this implementation, each host aggregates it's data and
 * sends it to the mpirun.  Once mpirun has gathered all the data it
 * broadcasts this data to all hosts.
 *     sendbuf - source buffer
 *     recvbug - destination buffer
 *     bytesPerProc - number of bytes each process contributes to the
 *        collective operation
 */
int adminMessage::allgather(void *sendbuf, void *recvbuf, ssize_t bytesPerProc)
{
    ssize_t recvCount = bytesPerProc;
    int returnCode = ULM_SUCCESS, typeTag, *dataArrivedFromHost;
    int nHostsArrived, host, hst;
    ssize_t totalBytes, bytesToCopy = 0;
    void *RESTRICT_MACRO recvBuff = recvbuf;
    void *RESTRICT_MACRO sendBuff = sendbuf;
    bool bReturnValue;
    void *src, *dest;
    size_t *aggregateData, offsetIntoAggregateData;
    recvResult recvReturnCode;
    long long tmpTag;
    int nLocalProcs, nStripesOnThisHost, *bytesLeftPerHost;

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = get_base_tag(1);

    int maxLocalProcs = 0;
    for (host = 0; host < nhosts_m; host++) {
        int nLocalProcs = groupHostData_m[host].nGroupProcIDOnHost;
        if (nLocalProcs > maxLocalProcs)
            maxLocalProcs = nLocalProcs;
    }
    ssize_t localStripeSize = lenSharedMemoryBuffer_m / nhosts_m;
    if (localStripeSize < 0) {
        ulm_err(("Error: adminMessage::allgather, localStripeSize = %lld\n",
                 (long long) localStripeSize));
        returnCode = ULM_ERROR;
        return returnCode;
    }
    ssize_t perRankStripeSize = localStripeSize / maxLocalProcs;
    if (perRankStripeSize < 0) {
        ulm_err(("Error: adminMessage::allgather, perRankStripeSize = %lld\n",
                 (long long) perRankStripeSize));
        returnCode = ULM_ERROR;
        return returnCode;
    }

    if (NULL == (bytesLeftPerHost = (int *) ulm_malloc(sizeof(int) * nhosts_m)))
        return ULM_ERROR;

    int numStripes = 0;
    for (host = 0; host < nhosts_m; host++) {
        nLocalProcs = groupHostData_m[host].nGroupProcIDOnHost;
        totalBytes = nLocalProcs * recvCount;
        bytesLeftPerHost[host] = totalBytes;
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
    if (server_m)
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
            size_t offsetIntoSharedBuffer = bytesToCopy * localProcessRank_m;

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
            *syncFlag_m = 0;
        }

        /* don't send data until all have written to the shared buffer
         */
        localBarrier();

        /*
         * Read the shared memory buffer - interhost data exchange
         */

        if (localProcessRank_m == hostCommRoot_m) {

            /* 
             * simple interhost accumlation of data 
             */

            /* send data to mpirun */
            bReturnValue = reset(adminMessage::SEND);
            if (!bReturnValue)
                return ULM_ERROR;
            bReturnValue = pack(&tag, LONGLONG, 1);
            if (!bReturnValue)
                return ULM_ERROR;
            bReturnValue = pack(sharedBuffer_m, BYTE,
                                bytesToCopy * groupHostData_m[hostRank_m].nGroupProcIDOnHost);
            if (!bReturnValue)
                return ULM_ERROR;

            totalBytes = totalNProcesses_m * bytesToCopy;
            bReturnValue = send(-1, adminMessage::ALLGATHER, &returnCode);
            if (!bReturnValue)
                return returnCode;

            /* receive the data from mpirun */
            /* compute how much data to receive */
            reset(adminMessage::RECEIVE, totalBytes);
            bReturnValue = receive(-1, &typeTag, &returnCode);
            if (!bReturnValue)
                return returnCode;
            if (typeTag != adminMessage::ALLGATHER) {
                ulm_err(("Error: unexpected tag in allgatherMultipleStripes.\n"
                         "\ttag received %d expected tag %d\n", typeTag, adminMessage::ALLGATHER));
                return returnCode;
            }

            bReturnValue = unpack(sharedBuffer_m, BYTE, totalBytes);
            if (!bReturnValue) {
                ulm_err(("Error: unpack returned error in allgatherMultipleStripes\n"));
                return returnCode;
            }

            /* memory barrier to ensure that all data has been written before setting flag */
            mb();

            /* set flag indicating data exchange is done */
            *syncFlag_m = 1;


        }
        /* spin until all data has been received */
        while (!(*syncFlag_m));

        /* Unpack the incoming data.  Data arrives sorted by host
         * index. e.g. host0, host1, host2, ...  within a given
         * host's data, this is arranged by local rank */

        /* the daemon process does not need this information */
        if (recvBuff) {
            ssize_t sharedMemBufOffset;
            for (int host = 0; host < nhosts_m; host++) {
                for (int lProc = 0; lProc < groupHostData_m[host].nGroupProcIDOnHost; lProc++) {
                    int proc = groupHostData_m[host].groupProcIDOnHost[lProc];
                    sharedMemBufOffset = proc * bytesToCopy;
                    dest = (void *) ((char *) recvBuff + bytesAlreadyCopied + proc * bytesPerProc);
                    src = (void *) ((char *) sharedBuffer_m + sharedMemBufOffset);
                    MEMCOPY_FUNC(src, dest, bytesToCopy);
                    sharedMemBufOffset += bytesToCopy;
                }
            }
        }

        /* end if ( recvBuff ) */
        /* spin until all data has been received */
        localBarrier();

    }                           /* end stripeID loop */

    /* done with client code */
    goto ReturnCode;

  ServerCode:

    aggregateData = (size_t *) ulm_malloc(bytesPerProc * totalNProcesses_m);
    dataArrivedFromHost = (int *) ulm_malloc(sizeof(int) * totalNProcesses_m);

    /* loop over data stripes */
    for (int stripeID = 0; stripeID < numStripes; stripeID++) {

        nHostsArrived = 0;
        for (host = 0; host < nhosts_m; host++) {
            dataArrivedFromHost[host] = 0;
        }
        ssize_t bytesAlreadyHandled = stripeID * perRankStripeSize;
        ssize_t leftToHandle = recvCount - bytesAlreadyHandled;
        ssize_t bytesToHandle = leftToHandle;
        if (bytesToHandle > perRankStripeSize)
            bytesToHandle = perRankStripeSize;

        /* loop until all data has arrived */
        while (nHostsArrived < nhosts_m) {

            /* loop over all hosts */
            for (host = 0; host < nhosts_m; host++) {

                /* continue if already received data from this host */
                if (dataArrivedFromHost[host])
                    continue;
                reset(adminMessage::RECEIVE);
                recvReturnCode = receive(host, &typeTag, &returnCode);

                /* no data to read  - continue */
                if (recvReturnCode == TIMEOUT) {
                    continue;
                }

                /* error */
                if (recvReturnCode == ERROR) {
                    ulm_err(("Error: from receive in adminMessage::allgather (%d)\n", returnCode));
                    returnCode = ULM_ERROR;
                    return recvReturnCode;
                }

                /* data ok */
                if (recvReturnCode == OK) {
                    /* make sure data is allgather data */
                    if (typeTag != ALLGATHER) {
                        ulm_err(("Error: Unexpected data type in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }

                    /* check tag */
                    bReturnValue = unpack(&tmpTag, (adminMessage::packType) sizeof(long long), 1);
                    if (!bReturnValue) {
                        ulm_err(("Error: from unpack in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }
                    if (tmpTag != tag) {
                        ulm_err(("Error: Tag mismatch in adminMessage::allgather\n"
                                 "\t Expected %lld - Arrived %lld\n", tag, tmpTag));
                    }
                    /* unpack data */
                    offsetIntoAggregateData = 0;
                    for (hst = 0; hst < host; hst++) {
                        offsetIntoAggregateData += (groupHostData_m[hst].nGroupProcIDOnHost *
                                                    bytesToHandle);
                    }
                    dest = ((char *) aggregateData) + offsetIntoAggregateData;
                    totalBytes = groupHostData_m[host].nGroupProcIDOnHost * bytesToHandle;
                    bReturnValue = unpack(dest, BYTE, totalBytes);
                    if (!bReturnValue) {
                        ulm_err(("Error: from unpack in adminMessage::allgather\n"));
                        return ULM_ERROR;
                    }

                    /* update received data counter */
                    nHostsArrived++;
                }
            }
        }                       /* end while (nHostsArrived<nhosts_m) loop */

        /* broadcast the data to the clients */
        totalBytes = bytesToHandle * totalNProcesses_m;
        bReturnValue = reset(adminMessage::SEND, totalBytes);
        if (!bReturnValue)
            return ULM_ERROR;

        bReturnValue = pack(aggregateData, BYTE, totalBytes);
        if (!bReturnValue)
            return ULM_ERROR;

        bReturnValue = broadcast(ALLGATHER, &returnCode);
        if (!bReturnValue) {
            ulm_err(("Error: from broadcast in adminMessage::allgather (%d)\n", returnCode));
            return ULM_ERROR;
        }

    }                           /* end loop over data stripes */

    ulm_free(aggregateData);
    ulm_free(dataArrivedFromHost);

  ReturnCode:

    return returnCode;
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
    while (*(barrierData_m->Counter) < barrierData_m->releaseCnt);

    return;
}


/* get a tag - not thread safe.  This is assumed to be used by the
 * library in a non-threaded region
 */
long long adminMessage::get_base_tag(int num_requested)
{
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
    int proc, host, count;
    int localCommSize = 0;

    /* set host comm-root */
    if (daemonIsHostCommRoot)
        hostCommRoot_m = DAEMON_PROC;
    else
        hostCommRoot_m = 0;

    /* set localProcessRank_m */
    localProcessRank_m = myLocalRank;

    /* 
     * set up groupHostData_m 
     */

    /* allocate memory */
    if (!groupHostData_m) {
        groupHostData_m = (admin_host_info_t *)
            ulm_malloc(nhosts_m * sizeof(admin_host_info_t));
        for (host = 0; host < nhosts_m; host++)
            groupHostData_m[host].groupProcIDOnHost = (int *) 0;

    }

    /* loop over host count */
    for (host = 0; host < nhosts_m; host++) {
        count = 0;
        /* figure out how many procs on given host */
        for (proc = 0; proc < totalNProcesses_m; proc++) {
            if (map_global_rank_to_host[proc] == host)
                count++;
        }
        /* set process count */
        groupHostData_m[host].nGroupProcIDOnHost = count;

        if (host == myHostRank)
            localCommSize = count;

        /* allocate memory for list of procs */
        if (groupHostData_m[host].groupProcIDOnHost)
            ulm_free(groupHostData_m[host].groupProcIDOnHost);
        groupHostData_m[host].groupProcIDOnHost = (int *)
            ulm_malloc(sizeof(int) * count);

        /* fill in list of procs */
        count = 0;
        for (proc = 0; proc < totalNProcesses_m; proc++) {
            if (map_global_rank_to_host[proc] == host) {
                groupHostData_m[host].groupProcIDOnHost[count] = proc;
                count++;
            }
        }
    }                           /* end host loop */

    /* 
     * initialize barrier structure 
     */

    barrierData_m = (swBarrierData *) ulm_malloc(sizeof(swBarrierData));
    if (daemonIsHostCommRoot)
        localCommSize++;
    barrierData_m->commSize = localCommSize;
    barrierData_m->releaseCnt = 0;

    /* finish setting barrier structures */
    barrierData_m->lock = (Locks *) lockPtr;
    barrierData_m->lock->init();

    /* set barrier Counter */
    barrierData_m->Counter = (volatile long long *) CounterPtr;
    *(barrierData_m->Counter) = 0;

    /* set collectiveTag_m */
    collectiveTag_m = -1;
    syncFlag_m = sPtr;

    /* set shared memory payload buffer */
    sharedBuffer_m = (void *) sharedBufferPtr;
    lenSharedMemoryBuffer_m = lenSMBuffer;

    return ULM_SUCCESS;
}


/* (server) initialize socket and wait for all connections from clients
 * timeout (in): time in seconds to wait for all connections from clients
 * returns: true if successful, false if unsuccessful (requiring program exit)
 */
bool adminMessage::serverConnect(int *procList, HostName_t * hostList, int numHosts, int timeout)
{
    int np = 0, nh = 0, tag, hostrank, nprocesses, recvAuthData[3], ok;
    int oldLCS = largestClientSocket_m, cnt;
    ulm_iovec_t iovecs[5];
    int size, *hostsAssigned = 0;
    pid_t daemonPid;

#if ENABLE_BPROC == 0
    int assignNewId;
    struct sockaddr_in addr;
#else
    int use_random_mapping = 0;
#endif

    /* initialization "stuff" */
    if (numHosts > 0) {
        hostsAssigned = (int *) ulm_malloc(numHosts * sizeof(int));
        if (!hostsAssigned) {
            ulm_err(("Error: Can't allocate memory for hostsAssigned list\n"));
            return false;
        }
        for (int i = 0; i < numHosts; i++)
            hostsAssigned[i] = 0;
    }

    if (timeout > 0) {
        if (sigaction(SIGALRM, &newSignals, &oldSignals) < 0) {
            return false;
        }
        if (setjmp(savedEnv) != 0) {
            struct sockaddr_in addr;
            struct hostent *h;

            ulm_err(("adminMessage::serverConnect timeout %d exceeded --"
                     " %d client sockets account for %d processes!\n"
                     TIMEOUT_HELP_STRING,
                     timeout, clientSocketCount(), clientProcessCount()));

            for (int i = 0; i < largestClientSocket_m; i++) {
                if (clientSocketActive_m[i]) {
                    socklen_t addrlen = sizeof(struct sockaddr_in);
                    int result = getpeername(i, (struct sockaddr *) &addr, &addrlen);
                    if (result == 0) {
                        h = gethostbyaddr((char *) (&addr.sin_addr.s_addr), 4, AF_INET);
                        ulm_err(("\tfd %d peer info: IP %s process count %d PID %ld\n", i,
                                 h ? h->h_name : inet_ntoa(addr.sin_addr), processCount[i],
                                 (long) daemonPIDs_m[i]));
                    }
                }
            }
            return false;
        }
        alarm(timeout);
    }
    // wait for all clients to connect

    // initialize processCount and daemonPIDs
    for (cnt = 0; cnt < MAXSOCKETS; cnt++) {
        processCount[cnt] = (int) 0;
        daemonPIDs_m[cnt] = (size_t) 0;
        socketsToProcess_m[cnt] = false;
    }

    while (np != totalNProcesses_m) {
        int sockfd = -1;
        struct sockaddr_in addr;
        socklen_t addrlen;
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(serverSocket_m, &fds);
        if (select(serverSocket_m + 1, &fds, (fd_set *) NULL, (fd_set *) NULL, NULL) < 0) {
            return false;;
        }

        addrlen = sizeof(addr);
        sockfd = accept(serverSocket_m, (struct sockaddr *) &addr, &addrlen);
        if (sockfd < 0) {
            continue;
        }

        if (sockfd >= adminMessage::MAXSOCKETS) {
            ulm_err(("adminMessage::serverConnect: client socket fd, %d, greater than "
                     "allowed MAXSOCKETS, %d\n", sockfd, adminMessage::MAXSOCKETS));
            close(sockfd);
        }

        // now do the authorization handshake...receive info
        iovecs[0].iov_base = &tag;
        iovecs[0].iov_len = (ssize_t) sizeof(int);
        iovecs[1].iov_base = recvAuthData;
        iovecs[1].iov_len = (ssize_t) (3 * sizeof(int));
        iovecs[2].iov_base = &hostrank;
        iovecs[2].iov_len = (ssize_t) sizeof(int);
        iovecs[3].iov_base = &nprocesses;
        iovecs[3].iov_len = (ssize_t) sizeof(int);
        iovecs[4].iov_base = &daemonPid;
        iovecs[4].iov_len = (ssize_t) sizeof(pid_t);
        if ((size = ulm_readv(sockfd, iovecs, 5)) != 6 * sizeof(int) + sizeof(pid_t)) {
            ulm_err(("adminMessage::serverConnect read from client socket failed!\n"));
            ulm_err(("Error: received %d expected %d\n", size, 6 * sizeof(int) + sizeof(pid_t)));
            close(sockfd);
            continue;
        }
#if ENABLE_BPROC
        /* BPROC node id from remote bproc_currnode() call must be translated */
        int hostrank_orig = hostrank;
        hostrank = nodeIDToHostRank(numHosts, hostList, hostrank);


        // first time, specify if we are using a random host mapping, or actual (default):
        if (nh == 0 && hostrank == numHosts)
            use_random_mapping = 1;

        if (use_random_mapping)
            ulm_err(("Using random host mapping: nh=%d client sent nodeid=%d\n", nh,
                     hostrank_orig));

        // in random host map mode, if any host returns valid hostrank, fail:
        // in actual host map mode, if any host returns invalid hostrank, fail:
        if ((hostrank == numHosts && use_random_mapping == 0)
            || (hostrank != numHosts && use_random_mapping == 1)) {
            ulm_err(("Error: adminMessage::serverConnect nodeIDToHostRank failed (sockfd = %d)!\n",
                     sockfd));
            if (timeout > 0) {
                alarm(0);
                sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
            }
            return false;
        }

        if (use_random_mapping == 1) {
            hostrank = nh;
        }
#else
        // set hostrank
        if (hostrank == UNKNOWN_HOST_ID) {
            assignNewId = 1;
            addrlen = sizeof(addr);
            getpeername(sockfd, (struct sockaddr *) &addr, &addrlen);
            hostrank = socketToHostRank(numHosts, hostList, &addr, assignNewId, hostsAssigned);
            if (hostrank == UNKNOWN_HOST_ID) {
                ulm_err(("Error: adminMessage::serverConnect UNKNOWN_HOST_ID (sockfd = %d)\n",
                         sockfd));
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
                }
                return false;
            }
        }
#endif

        // cache process count
        if (nprocesses != UNKNOWN_N_PROCS) {
            processCount[hostrank] = nprocesses;
        } else {
            if (hostrank == UNKNOWN_HOST_ID) {
                ulm_err(("Error: adminMessage::serverConnect UNKNOWN_HOST_ID (sockfd = %d)\n",
                         sockfd));
                if (timeout > 0) {
                    alarm(0);
                    sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
                }
                return false;
            }
        }

        // cache process count
        if (nprocesses != UNKNOWN_N_PROCS) {
            processCount[hostrank] = nprocesses;
        } else {
            processCount[hostrank] = procList[hostrank];
            nprocesses = procList[hostrank];
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
        daemonPIDs_m[hostrank] = daemonPid;

        // send reply INITOK message
        tag = INITOK;
        iovecs[0].iov_base = &tag;
        iovecs[0].iov_len = (ssize_t) sizeof(int);
        iovecs[1].iov_base = authData_m;
        iovecs[1].iov_len = (ssize_t) (3 * sizeof(int));
        iovecs[2].iov_base = &ok;
        iovecs[2].iov_len = (ssize_t) sizeof(int);
        if (ulm_writev(sockfd, iovecs, 3) != 5 * sizeof(int)) {
            ulm_err(("adminMessage::serverConnect write to client socket failed!\n"));
            close(sockfd);
            if (ok == 1) {
                np -= nprocesses;
                clientSocketActive_m[sockfd] = false;
                largestClientSocket_m = oldLCS;
            }
            continue;
        }
    }                           /* end while (np != totalNProcesses_m) */

    if (timeout > 0) {
        alarm(0);
        sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
    }

    nhosts_m = clientSocketCount();

    if (numHosts > 0)
        ulm_free(hostsAssigned);

    return true;
}


/* send data from send buffer (or receive buffer if useRecvBuffer is true) to all destinations
 * tag (in): message tag value
 * errorCode (out): errorCode if false is returned
 * returns: true if successful, false if unsuccessful (errorCode is then set)
 */
bool adminMessage::broadcast(int tag, int *errorCode)
{
    bool returnValue = true;
    ulm_iovec_t iovecs[2];
    int bytesToWrite, bytesWritten;

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



bool adminMessage::send(int rank, int tag, int *errorCode)
{
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
    if (ulm_writev(sockfd, iovecs, (sendOffset_m) ? 2 : 1) != (int) (sendOffset_m + sizeof(int))) {
        *errorCode = errno;
        returnValue = false;
        return returnValue;
    }

    return returnValue;
}


bool adminMessage::reset(direction dir, int size)
{
    bool returnValue = true;

    if ((dir == SEND) || (dir == BOTH)) {
        sendOffset_m = 0;
        if (size && (size > sendBufferSize_m)) {
            unsigned char *tmp = (unsigned char *) realloc(sendBuffer_m, size);
            if (tmp) {
                sendBufferSize_m = size;
                sendBuffer_m = tmp;
            } else {
                returnValue = false;
            }
        }
    }

    if ((dir == RECEIVE) || (dir == BOTH)) {
        recvOffset_m = 0;
        recvBufferBytes_m = 0;
        recvMessageBytes_m = 0;
        if (size && (size > recvBufferSize_m)) {
            unsigned char *tmp = (unsigned char *) realloc(recvBuffer_m, size);
            if (tmp) {
                recvBufferSize_m = size;
                recvBuffer_m = tmp;
            } else {
                returnValue = false;
            }
        }
    }
    return returnValue;
}


bool adminMessage::pack(void *data, packType type, int count, packFunction pf, packSizeFunction psf)
{
    bool returnValue = true;
    void *dst = (void *) (sendBuffer_m + sendOffset_m);

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
            if ((int) (sendOffset_m + count * sizeof(unsigned char)) > sendBufferSize_m) {
                ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n", type, sendOffset_m + count * sizeof(unsigned char)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(data, dst, sizeof(unsigned char) * count);
                sendOffset_m += count * sizeof(unsigned char);
            }
        }
        break;
    case SHORT:
        {
            if ((int) (sendOffset_m + count * sizeof(unsigned short)) > sendBufferSize_m) {
                ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n", type, sendOffset_m + count * sizeof(unsigned short)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(data, dst, sizeof(unsigned short) * count);
                sendOffset_m += count * sizeof(unsigned short);
            }
        }
        break;
    case INTEGER:
        {
            if ((int) (sendOffset_m + count * sizeof(unsigned int)) > sendBufferSize_m) {
                ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n", type, sendOffset_m + count * sizeof(unsigned int)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(data, dst, sizeof(unsigned int) * count);
                sendOffset_m += count * sizeof(unsigned int);
            }
        }
        break;
    case LONGLONG:
        {
            if ((int) (sendOffset_m + count * sizeof(unsigned long long)) > sendBufferSize_m) {
                ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n", type, sendOffset_m + count * sizeof(unsigned long long)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(data, dst, sizeof(unsigned long long) * count);
                sendOffset_m += count * sizeof(unsigned long long);
            }
        }
        break;
    case USERDEFINED:
        {
            if (!psf || !pf) {
                ulm_err(("adminMessage:pack pack size function (psf = %p) and pack function (pf = %p)\n", psf, pf));
                returnValue = false;
                return returnValue;
            }
            int neededBytes = (*psf) (count);
            if ((sendOffset_m + neededBytes) > sendBufferSize_m) {
                ulm_err(("adminMessage::pack too much data (type %d), need %d bytes in send buffer\n", type, sendOffset_m + neededBytes));
                returnValue = false;
            } else {
                (*pf) (data, dst, count);
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
    if (true == unpackMessage(tag, INTEGER, 1))
        return OK;
    else
        return ERROR;
}


bool adminMessage::unpackMessage(void *data, packType type, int count, int timeout,
                                 unpackFunction upf, unpackSizeFunction upsf)
{
    bool returnValue = true;
    void *src = (void *) (recvBuffer_m);

    if (count <= 0)
        return returnValue;

    if (!data) {
        ulm_err(("adminMessage::unpack data is NULL pointer (count = %d)\n", count));
        returnValue = false;
        return returnValue;
    }
    // repack the receive buffer, if needed
    if (recvOffset_m && (recvBufferBytes_m > recvOffset_m)) {
        memmove(recvBuffer_m, (void *) (recvBuffer_m + recvOffset_m),
                recvBufferBytes_m - recvOffset_m);
        recvBufferBytes_m -= recvOffset_m;
        recvOffset_m = 0;
    }

    switch (type) {
    case BYTE:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned char)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned char)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(src, data, sizeof(unsigned char) * count);
                recvOffset_m += count * sizeof(unsigned char);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
        }
        break;
    case SHORT:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned short)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned short)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(src, data, sizeof(unsigned short) * count);
                recvOffset_m += count * sizeof(unsigned short);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
        }
        break;
    case INTEGER:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned int)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned int)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(src, data, sizeof(unsigned int) * count);
                recvOffset_m += count * sizeof(unsigned int);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
        }
        break;
    case LONGLONG:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned long long)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned long long)));
                returnValue = false;
            } else {
                MEMCOPY_FUNC(src, data, sizeof(unsigned long long) * count);
                recvOffset_m += count * sizeof(unsigned long long);
                if (recvOffset_m == recvBufferBytes_m) {
                    recvOffset_m = recvBufferBytes_m = 0;
                }
            }
        }
        break;
    case USERDEFINED:
        {
            if (!upsf || !upf) {
                ulm_err(("adminMessage:unpack unpack size function (upsf = %p) and unpack function (upf = %p)\n", upsf, upf));
                returnValue = false;
                return returnValue;
            }
            int neededBytes = (*upsf) (count);
            if ((recvOffset_m + neededBytes) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + neededBytes));
                returnValue = false;
            } else {
                (*upf) (recvBuffer_m, data, count);
                recvOffset_m += neededBytes;
                if (recvOffset_m == recvBufferBytes_m) {
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
                          unpackSizeFunction upsf)
{
    bool returnValue = true, gotData = true;
    void *src = (void *) (recvBuffer_m);

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
        memmove(recvBuffer_m, (void *) (recvBuffer_m + recvOffset_m),
                recvBufferBytes_m - recvOffset_m);
        recvBufferBytes_m -= recvOffset_m;
        recvOffset_m = 0;
    }

    switch (type) {
    case BYTE:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned char)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned char)));
                returnValue = false;
            } else {
                if (recvBufferBytes_m < (int) (count * sizeof(unsigned char))) {
                    gotData =
                        getRecvBytes(count * sizeof(unsigned char) - recvBufferBytes_m, timeout);
                }
                if (gotData) {
                    MEMCOPY_FUNC(src, data, sizeof(unsigned char) * count);
                    recvOffset_m += count * sizeof(unsigned char);
                    if (recvOffset_m == recvBufferBytes_m) {
                        recvOffset_m = recvBufferBytes_m = 0;
                    }
                } else {
                    ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d " "bytes, timeout %d seconds)\n", count * sizeof(unsigned char), recvBufferBytes_m, timeout));
                    returnValue = false;
                }
            }
        }
        break;
    case SHORT:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned short)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned short)));
                returnValue = false;
            } else {
                if (recvBufferBytes_m < (int) (count * sizeof(unsigned short))) {
                    gotData =
                        getRecvBytes(count * sizeof(unsigned short) - recvBufferBytes_m, timeout);
                }
                if (gotData) {
                    MEMCOPY_FUNC(src, data, sizeof(unsigned short) * count);
                    recvOffset_m += count * sizeof(unsigned short);
                    if (recvOffset_m == recvBufferBytes_m) {
                        recvOffset_m = recvBufferBytes_m = 0;
                    }
                } else {
                    ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d " "bytes, timeout %d seconds)\n", count * sizeof(unsigned short), recvBufferBytes_m, timeout));
                    returnValue = false;
                }
            }
        }
        break;
    case INTEGER:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned int)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned int)));
                returnValue = false;
            } else {
                if (recvBufferBytes_m < (int) (count * sizeof(unsigned int))) {
                    gotData =
                        getRecvBytes(count * sizeof(unsigned int) - recvBufferBytes_m, timeout);
                }
                if (gotData) {
                    MEMCOPY_FUNC(src, data, sizeof(unsigned int) * count);
                    recvOffset_m += count * sizeof(unsigned int);
                    if (recvOffset_m == recvBufferBytes_m) {
                        recvOffset_m = recvBufferBytes_m = 0;
                    }
                } else {
                    ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d " "bytes, timeout %d seconds)\n", count * sizeof(unsigned int), recvBufferBytes_m, timeout));
                    returnValue = false;
                }
            }
        }
        break;
    case LONGLONG:
        {
            if ((int) (recvOffset_m + count * sizeof(unsigned long long)) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + count * sizeof(unsigned long long)));
                returnValue = false;
            } else {
                if (recvBufferBytes_m < (int) (count * sizeof(unsigned long long))) {
                    gotData =
                        getRecvBytes(count * sizeof(unsigned long long) - recvBufferBytes_m,
                                     timeout);
                }
                if (gotData) {
                    MEMCOPY_FUNC(src, data, sizeof(unsigned long long) * count);
                    recvOffset_m += count * sizeof(unsigned long long);
                    if (recvOffset_m == recvBufferBytes_m) {
                        recvOffset_m = recvBufferBytes_m = 0;
                    }
                } else {
                    ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d " "bytes, timeout %d seconds)\n", count * sizeof(unsigned long long), recvBufferBytes_m, timeout));
                    returnValue = false;
                }
            }
        }
        break;
    case USERDEFINED:
        {
            if (!upsf || !upf) {
                ulm_err(("adminMessage:unpack unpack size function (upsf = %p) and unpack function (upf = %p)\n", upsf, upf));
                returnValue = false;
                return returnValue;
            }
            int neededBytes = (*upsf) (count);
            if ((recvOffset_m + neededBytes) > recvBufferSize_m) {
                ulm_err(("adminMessage::unpack too much data (type %d), need %d bytes in receive buffer\n", type, recvOffset_m + neededBytes));
                returnValue = false;
            } else {
                if (recvBufferBytes_m < neededBytes) {
                    gotData = getRecvBytes(neededBytes - recvBufferBytes_m, timeout);
                }
                if (gotData) {
                    (*upf) (recvBuffer_m, data, count);
                    recvOffset_m += neededBytes;
                    if (recvOffset_m == recvBufferBytes_m) {
                        recvOffset_m = recvBufferBytes_m = 0;
                    }
                } else {
                    ulm_err(("adminMessage::unpack unable to get all of data (need %d bytes, have %d " "bytes, timeout %d seconds)\n", neededBytes, recvBufferBytes_m, timeout));
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
adminMessage::recvResult adminMessage::receiveFromAny(int *rank, int *tag, int *errorCode,
                                                      int timeout)
{
    recvResult returnValue = OK;
    int sockfd = -1, maxfd = 0, s;
    struct timeval t;
    ulm_fd_set_t fds;
    ulm_iovec_t iovecs;

    reset(RECEIVE);

    if (timeout >= 0) {
        int seconds = timeout / (int) 1000;
        int microseconds = timeout * 1000;
        if (seconds) {
            microseconds = (timeout - (seconds * 1000)) * 1000;
        }
        t.tv_sec = seconds;
        t.tv_usec = microseconds;
    }

    bzero(&fds, sizeof(fds));
    if ((client_m) && (socketToServer_m >= 0)) {
        FD_SET(socketToServer_m, (fd_set *) & fds);
        maxfd = socketToServer_m;
    }
    if (server_m) {
        for (int i = 0; i <= largestClientSocket_m; i++) {
            if (clientSocketActive_m[i]) {
                FD_SET(i, (fd_set *) & fds);
                if (i > maxfd) {
                    maxfd = i;
                }
            }
        }
    }

    if ((s = select(maxfd + 1, (fd_set *) & fds, (fd_set *) NULL, (fd_set *) NULL,
                    (timeout < 0) ? (struct timeval *) NULL : &t)) > 0) {
        iovecs.iov_base = tag;
        iovecs.iov_len = sizeof(int);
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, (fd_set *) & fds)) {
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
    } else {
        returnValue = (s == 0) ? TIMEOUT : ERROR;
        if (returnValue == ERROR)
            *errorCode = errno;
        return returnValue;
    }

    if (callbacks_m[*tag]) {
        bool callReturn = (*callbacks_m[*tag]) (this, *rank, *tag);
        if (callReturn) {
            returnValue = HANDLED;
        } else {
            returnValue = ERROR;
            *errorCode = ULM_ERROR;
        }
    }

    return returnValue;
}


/* return true if the name or IP address in dot notation of the peer
 * to be stored at dst, or false if there is an error */
bool adminMessage::peerName(int hostrank, char *dst, int bytes, bool useDottedIP)
{
    int len;
    char *dotted;
    int sockfd = (hostrank == -1) ? socketToServer_m : clientRank2FD(hostrank);
    struct sockaddr_in addr;
    struct hostent *h;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if (sockfd < 0) {
        return false;
    }

    if (getpeername(sockfd, (struct sockaddr *) &addr, &addrlen) != 0) {
        return false;
    }

    if (useDottedIP) {
        // return the peerName using the dotted IP address.
        dotted = inet_ntoa(addr.sin_addr);
        len = strlen(dotted);
        bzero(dst, bytes);
        if (len <= (ssize_t) (bytes - 1)) {
            strcpy(dst, dotted);
        } else {
            return false;
        }
    } else {
        h = gethostbyaddr((char *) (&addr.sin_addr.s_addr), 4, AF_INET);
        dst[0] = '\0';
        if (h != (struct hostent *) NULL) {
            if (strlen(h->h_name) <= (size_t) (bytes - 1)) {
                strcpy(dst, h->h_name);
            } else if (strlen(inet_ntoa(addr.sin_addr)) <= (size_t) (bytes - 1)) {
                strcpy(dst, inet_ntoa(addr.sin_addr));
            } else {
                return false;
            }
        } else {
            if (strlen(inet_ntoa(addr.sin_addr)) <= (size_t) (bytes - 1)) {
                strcpy(dst, inet_ntoa(addr.sin_addr));
            } else {
                return false;
            }
        }
    }                           // if ( useDottedIP )

    return true;
}


void adminMessage::setHostRank(int rank)
{
    hostRank_m = rank;
}


admin_host_info_t *adminMessage::groupHostData()
{
    return groupHostData_m;
}


void adminMessage::setGroupHostData(admin_host_info_t * groupHostData)
{
    if (groupHostData_m) {
        ulm_free2(groupHostData_m->groupProcIDOnHost);
        ulm_free2(groupHostData_m);
    }
    groupHostData_m = groupHostData;
}


ssize_t adminMessage::lenSharedMemoryBuffer()
{
    return lenSharedMemoryBuffer_m;
}


void adminMessage::setLenSharedMemoryBuffer(ssize_t len)
{
    lenSharedMemoryBuffer_m = len;
}


int adminMessage::localProcessRank()
{
    return localProcessRank_m;
}


void adminMessage::setLocalProcessRank(int rank)
{
    localProcessRank_m = rank;
}


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
