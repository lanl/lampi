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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>             // needed for bzero()
#include <sys/time.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "internal/log.h"
#include "client/ULMClient.h"
#include "client/SocketGeneric.h"
#include "queue/globals.h"

/*
 * Receive and process control messages from mpirun
 */
int ClientCheckForControlMsgs(int MaxDescriptor,
                              int *ServerSocketFD,
                              double *HeartBeatTime,
                              int *ProcessCount,
                              int hostIndex,
                              pid_t *ChildPIDs,
                              int *STDINfdToChild,
                              int *STDOUTfdsFromChildren,
                              int *STDERRfdsFromChildren, 
                              size_t *StderrBytesWritten,
                              size_t *StdoutBytesWritten,
                              int *NewLineLast,
                              PrefixName_t *IOPreFix,
                              int *LenIOPreFix,
                              lampiState_t *state)
{
    fd_set ReadSet;
    int rc, NotifyServer, NFDs, MaxDesc, error;
    ssize_t size;
    struct timeval WaitTime;
    ulm_iovec_t iovec;
    unsigned int message;
    unsigned int tag;

    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 100000;

    if ((*ServerSocketFD) == -1) {
        return 0;
    }

    FD_ZERO(&ReadSet);
    if (*ServerSocketFD >= 0) {
        FD_SET(*ServerSocketFD, &ReadSet);
    }

    rc = select(MaxDescriptor, &ReadSet, NULL, NULL, &WaitTime);
    if (rc <= 0) {
        return rc;
    }

    if (((*ServerSocketFD) >= 0) && (FD_ISSET(*ServerSocketFD, &ReadSet))) {

        /* read tag value */
        tag = 0;
        size = RecvSocket(*ServerSocketFD, &tag, sizeof(unsigned int), &error);

        /* socket connection closed */
        if (size == 0) {
            *ServerSocketFD = -1;
            return 0;
        }
        if (size < 0 || error != ULM_SUCCESS) {
            ulm_exit(("Error: reading tag.  rc = %ld, "
                      "error = %d\n", size, error));
        }

        /* process according to message type */
        switch (tag) {

        case HEARTBEAT:
#ifndef HAVE_CLOCK_GETTIME
            struct timeval t;
            gettimeofday(&t, NULL);
            *HeartBeatTime = (double) t.tv_sec + ((double) t.tv_usec) * 1e-6;
#else
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            *HeartBeatTime = (double) t.tv_sec + ((double) t.tv_nsec) * 1e-9;
#endif
            break;

        case TERMINATENOW:
            /* recieve request to teminate immediately */
            message = ACKTERMINATENOW;
            NotifyServer = 1;
            ClientAbort(*ServerSocketFD, ProcessCount, hostIndex,
                        ChildPIDs, message, NotifyServer);
            break;

        case ALLHOSTSDONE:
            /* set flag indicating app process can terminate */
            ulm_dbg(("client ALLHOSTSDONE arrived on host %d\n", hostIndex));
            tag = ACKALLHOSTSDONE;
            iovec.iov_base = (char *) &tag;
            iovec.iov_len = (ssize_t) sizeof(unsigned int);
            size = SendSocket(*ServerSocketFD, 1, &iovec);
            if (size < 0) {
                ulm_exit(("Error: sending ACKALLHOSTSDONE.  "
                          "rc: %ld\n", size));
            }
            /* drain stdio */
            MaxDesc = 0;
            NFDs = ProcessCount[hostIndex] + 1;
            for (int i = 0; i < NFDs; i++) {
                if (STDOUTfdsFromChildren[i] > MaxDesc) {
                    MaxDesc = STDOUTfdsFromChildren[i];
                }
                if (STDERRfdsFromChildren[i] > MaxDesc) {
                    MaxDesc = STDERRfdsFromChildren[i];
                }
            }
            MaxDesc++;
            ClientDrainSTDIO(STDOUTfdsFromChildren,
                             STDERRfdsFromChildren, *ServerSocketFD,
                             NFDs, MaxDesc, IOPreFix, LenIOPreFix,
                             StderrBytesWritten, StdoutBytesWritten,
                             NewLineLast, state);
            *lampiState.sync.AllHostsDone = 1;

            // ok, the client daemon can now exit...
            exit(EXIT_SUCCESS);
            break;

        case STDIOMSG:
            if (*STDINfdToChild >= 0) {
                ClientRecvStdin(ServerSocketFD, STDINfdToChild);
            }
            break;

        default:
            ulm_exit(("Error: Unrecognized control message (%u)\n", tag));
        }
    }

    return 0;
}
