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



#include "internal/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>

#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/types.h"
#include "client/SocketGeneric.h"
#include "client/SocketServer.h"
#include "internal/new.h"

#ifdef BPROC
#include "sys/bproc.h"
#endif

#define MAX_TRIES 1000

int *PortList;

int SetupAcceptingSocket(int *SocketReturn)
{
    int PortID;
#ifdef __linux__
    socklen_t sockbuf;
#else
    int sockbuf;
#endif
    struct sockaddr_in tmpHostInfo;

    bzero(&tmpHostInfo, sizeof(struct sockaddr_in));
    PortID = 7100;
    if ((*SocketReturn = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    sockbuf = 1;
    setsockopt((*SocketReturn), IPPROTO_TCP, TCP_NODELAY, &sockbuf,
               sizeof(int));

    tmpHostInfo.sin_family = AF_INET;
    tmpHostInfo.sin_addr.s_addr = INADDR_ANY;

    if ((bind(*SocketReturn, (struct sockaddr *) &tmpHostInfo,
              sizeof(struct sockaddr_in))) < 0) {
        return -2;
    }
    sockbuf = sizeof(tmpHostInfo);
    if (getsockname(*SocketReturn, (sockaddr *) & tmpHostInfo, &sockbuf) <
        0)
        return -4;
    PortID = ntohs(tmpHostInfo.sin_port);
    if (listen(*SocketReturn, SOMAXCONN) < 0)
        return -3;

    return PortID;
}

/*
 * This routine is used to recieve socket connection from
 * NClients from the ClientList
 */

int AcceptSocketConnections(int SocketStart, int NClientsSpawned,
                            int *NClientsAccepted,
                            int **ListClientsAccepted,
                            ULMRunParams_t *RunParameters)
{
    int i, j, SocketTmp, RetVal;
#ifdef __linux__
    socklen_t len;
#else
    int len;
#endif                          /* LINUX */
    struct sockaddr_in Child;
#ifndef BPROC
    struct hostent *TmpHost;
#endif
    unsigned int AuthData[3];

    /* initialize data */
    RunParameters->Networks.TCPAdminstrativeNetwork.SocketsToClients =
        (int *) ulm_malloc(sizeof(int) * RunParameters->NHosts);
    for (i = 0; i < RunParameters->NHosts; i++)
        RunParameters->Networks.TCPAdminstrativeNetwork.
            SocketsToClients[i] = -1;

    *NClientsAccepted = 0;
    *ListClientsAccepted = ulm_new(int, NClientsSpawned);
    for (i = 0; i < NClientsSpawned; i++)
        (*ListClientsAccepted)[i] = -1;

    /* accept connections */
    for (i = 0; i < NClientsSpawned; i++) {
        len = sizeof(struct sockaddr_in);
        while (((SocketTmp =
                 accept(SocketStart, (struct sockaddr *) &Child,
                        &len)) == -1)
               && (errno == EINTR));
        if (SocketTmp < 0) {
            perror(" accept ");
            return -1;
        }
        /* Sending host */
#ifdef BPROC
        int size = sizeof(struct sockaddr);
        int nodeID = bproc_nodenumber((struct sockaddr *) &Child, size);
        /* find order in list */
        for (j = 0; j < RunParameters->NHosts; j++) {
            int node = bproc_getnodebyname(RunParameters->HostList[j]);
            int foundNode = 0;
            if (node == nodeID)
                foundNode = 1;
            if ((foundNode)
                && (RunParameters->Networks.TCPAdminstrativeNetwork.
                    SocketsToClients[j] == -1)) {
                /* hand shake */
                ulm_GetAuthString(AuthData);
                RetVal = MPIrunHandshakeWithClient(SocketTmp, AuthData);
                if (RetVal == 0) {
                    RunParameters->Networks.TCPAdminstrativeNetwork.
                        SocketsToClients[j] = SocketTmp;
                    (*ListClientsAccepted)[*NClientsAccepted] = j;
                    (*NClientsAccepted)++;
                }
                break;
            }
        }                       /* end j loop */
#else

        TmpHost =
            gethostbyaddr((char *) &(Child.sin_addr.s_addr), 4, AF_INET);
        /* find order in list */
        for (j = 0; j < RunParameters->NHosts; j++) {
            RetVal =
                strncmp(RunParameters->HostList[j], TmpHost->h_name,
                        strlen(TmpHost->h_name));
            if ((RetVal == 0)
                && (RunParameters->Networks.TCPAdminstrativeNetwork.
                    SocketsToClients[j] == -1)) {
                /* hand shake */
                ulm_GetAuthString(AuthData);
                RetVal = MPIrunHandshakeWithClient(SocketTmp, AuthData);
                if (RetVal == 0) {
                    RunParameters->Networks.TCPAdminstrativeNetwork.
                        SocketsToClients[j] = SocketTmp;
                    (*ListClientsAccepted)[*NClientsAccepted] = j;
                    (*NClientsAccepted)++;
                }
                break;
            }
        }                       /* end j loop */
#endif
        if (SocketTmp == -1)
            return -2;
    }                           /* end i loop */

    return 0;
}
