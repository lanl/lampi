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
#include <unistd.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "client/SocketServer.h"

int ClientHandshakeWithServer(int Socket, unsigned int *AuthData)
{
    int i;
    ssize_t IOReturn;
    unsigned int RecvAuthData[3], Tag;
    ulm_iovec_t RecvIovec[2];
    ulm_iovec_t SendIovec[2];

    /* send auth string to Server */
    Tag = AUTHDATA;
    SendIovec[0].iov_base = &Tag;
    SendIovec[0].iov_len = (ssize_t) (sizeof(unsigned int));
    SendIovec[1].iov_base = AuthData;
    SendIovec[1].iov_len = (ssize_t) (3 * sizeof(unsigned int));
    IOReturn = ulm_writev(Socket, SendIovec, 2);
    if (IOReturn != (4 * sizeof(unsigned int))) {
        close(Socket);
        ulm_exit((-1, "Error: Sending authorization data to server\n"));
    }

    /* recieve auth string from Client - block until this is received */
#ifdef PURIFY
    Tag = (unsigned int) 0;
    RecvAuthData[0] = (unsigned int) 0;
    RecvAuthData[1] = (unsigned int) 0;
    RecvAuthData[2] = (unsigned int) 0;
#endif
    RecvIovec[0].iov_base = &Tag;
    RecvIovec[0].iov_len = (ssize_t) (sizeof(unsigned int));
    RecvIovec[1].iov_base = RecvAuthData;
    RecvIovec[1].iov_len = (ssize_t) (3 * sizeof(unsigned int));
    IOReturn = ulm_readv(Socket, RecvIovec, 2);
    if (IOReturn != (4 * sizeof(unsigned int))) {
        close(Socket);
        return -1;
    }
    if (Tag != AUTHDATA) {
        close(Socket);
        ulm_exit((-1, "Error: Expected AUTHDATA tag\n"));
    }
    for (i = 0; i < 3; i++) {
        if (RecvAuthData[i] != AuthData[i]) {
            close(Socket);
            ulm_exit((-1, "Error: Authorization string\n"));
        }
    }

    return 0;
}
