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

#if ENABLE_BPROC
#include <sys/bproc.h>
#endif

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/SocketGeneric.h"
#include "client/SocketClient.h"
#include "ulm/errors.h"

/*
 * This routine connects the sever process to
 * a well known (ServerPortNumber) on the ServerHost
 */
int SocketConnectToServer(int ServerPortNumber, HostName_t ServerHost,
                          int *ClientSocketFD)
{
    int RetVal, sockbuf;
    unsigned int AuthData[3];
    struct sockaddr_in ParentSockAddr;
    struct hostent *TmpHost;

    /* open socket */
    (*ClientSocketFD) = socket(AF_INET, SOCK_STREAM, 0);
    if ((*ClientSocketFD) < 0) {
        ulm_err(("Error: from the socket call (%d)\n",
                 *ClientSocketFD));
        return -4;
    }

    sockbuf = 1;
    setsockopt((*ClientSocketFD), IPPROTO_TCP, TCP_NODELAY, &sockbuf,
               sizeof(int));

    TmpHost = gethostbyname(ServerHost);
#if ENABLE_BPROC
    if (TmpHost == (struct hostent *)NULL) {
        int size = sizeof(struct sockaddr);
        RetVal = bproc_nodeaddr(BPROC_NODE_MASTER,
            (struct sockaddr *) &ParentSockAddr, &size);
        if (RetVal != 0) {
            ulm_err(("Error: from bproc_nodeaddr (%d)\n", errno));
            return ULM_ERROR;
        }
    }
    else {
        memcpy((char *) &ParentSockAddr.sin_addr, TmpHost->h_addr_list[0],
           TmpHost->h_length);
    }
#else
    memcpy((char *) &ParentSockAddr.sin_addr, TmpHost->h_addr_list[0],
           TmpHost->h_length);
#endif
    /* use "well known" port on server */
    ParentSockAddr.sin_port = htons((unsigned short) ServerPortNumber);
    ParentSockAddr.sin_family = AF_INET;

    /* connect to "well known" port on server */
    while (((RetVal =
             connect((*ClientSocketFD),
                     (struct sockaddr *) &ParentSockAddr,
                     sizeof(struct sockaddr_in))) == -1)
           && (errno == EINTR));
    if (RetVal == -1) {
        ulm_err(("Error: returned from the connect call :: %d fd %d errno %d\n", RetVal, *ClientSocketFD, errno));
        return -5;
    }
    /* Hand Shake with server */
    ulm_GetAuthString(AuthData);
    ClientHandshakeWithServer(*ClientSocketFD, AuthData);

    return 0;
}
