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
 * this routine receives and process control messages from the server
 */
int ClientCheckForControlMsgs(int MaxDescriptor, int *ServerSocketFD,
                              double *HeartBeatTime, int *ProcessCount,
                              int hostIndex, pid_t *ChildPIDs,
                              int *STDINfdToChild,
                              int *STDOUTfdsFromChildren,
                              int *STDERRfdsFromChildren, 
                              size_t *StderrBytesWritten,
                              size_t *StdoutBytesWritten,
                              int *NewLineLast, PrefixName_t *IOPreFix,
                              int *LenIOPreFix, lampiState_t *state)
{
    fd_set ReadSet;
    int i, RetVal, NotifyServer, NFDs, MaxDesc, error;
    double Message;
    struct timeval WaitTime;
    unsigned int Tag;
    ssize_t IOReturn;
    ulm_iovec_t IOVec;
#ifndef HAVE_CLOCK_GETTIME
    struct timeval Time;
#else
    struct timespec Time;
#endif                          /* LINUX */

    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 0;

    if ((*ServerSocketFD) == -1)
        return 0;
    FD_ZERO(&ReadSet);
    if (*ServerSocketFD >= 0)
        FD_SET(*ServerSocketFD, &ReadSet);

    RetVal = select(MaxDescriptor, &ReadSet, NULL, NULL, &WaitTime);
    if (RetVal < 0)
        return RetVal;
    if (RetVal > 0) {
        if (((*ServerSocketFD) >= 0)
            && (FD_ISSET(*ServerSocketFD, &ReadSet))) {
            /* Read tag value */
            Tag = 0;
            IOReturn = RecvSocket(*ServerSocketFD, &Tag,
                                  sizeof(unsigned int), &error);
            /* socket connection closed */
            if (IOReturn == 0) {
                *ServerSocketFD = -1;
                return 0;
            }
            if (IOReturn < 0 || error != ULM_SUCCESS) {
                ulm_exit((-1, "Error: reading Tag.  RetVal = %ld, "
                          "error = %d\n", IOReturn, error));
            }
            switch (Tag) {      /* process according to message type */
            case HEARTBEAT:
#ifndef HAVE_CLOCK_GETTIME
                gettimeofday(&Time, NULL);
                *HeartBeatTime =
                    (double) (Time.tv_sec) +
                    ((double) Time.tv_usec) * 1e-6;
#else
                clock_gettime(CLOCK_REALTIME, &Time);
                *HeartBeatTime =
                    (double) (Time.tv_sec) +
                    ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
                break;
            case ACKNORMALTERM:
                Tag = ACKACKNORMALTERM;
                IOVec.iov_base = (char *) &Tag;
                IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                /* send ack of ack so that mpirun can shut down - this
                   is done to let mpirun have time to read all stdio
                   from the client hosts */
                IOReturn = SendSocket(*ServerSocketFD, 1, &IOVec);
                if (IOReturn < 0) {
                    ulm_exit((-1, "Error: sending ACKACKNORMALTERM.  "
                              "RetVal: %ld\n", IOReturn));
                }
                break;
            case TERMINATENOW:
                /* recieve request to teminate immediately */
                Message = ACKTERMINATENOW;
                NotifyServer = 1;
                AbortAndDrainLocalHost(*ServerSocketFD, ProcessCount,
                                       hostIndex, ChildPIDs,
                                       (unsigned int) Message,
                                       NotifyServer, STDOUTfdsFromChildren,
                                       STDERRfdsFromChildren, IOPreFix,
                                       LenIOPreFix, StderrBytesWritten,
                                       StdoutBytesWritten, NewLineLast,
                                       state);
                break;
            case ACKABNORMALTERM:
                Tag = ACKACKABNORMALTERM;
                IOVec.iov_base = (char *) &Tag;
                IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                /* send ack of ack so that mpirun can shut down */
                IOReturn = SendSocket(*ServerSocketFD, 1, &IOVec);
                /* drain stdio */
                MaxDesc = 0;
                NFDs = ProcessCount[hostIndex] + 1;
                for (i = 0; i < NFDs; i++) {
                    if (STDOUTfdsFromChildren[i] > MaxDesc)
                        MaxDesc = STDOUTfdsFromChildren[i];
                    if (STDERRfdsFromChildren[i] > MaxDesc)
                        MaxDesc = STDERRfdsFromChildren[i];
                }
                MaxDesc++;
                ClientDrainSTDIO(STDOUTfdsFromChildren,
                                 STDERRfdsFromChildren, *ServerSocketFD,
                                 NFDs, MaxDesc, IOPreFix, LenIOPreFix,
                                 StderrBytesWritten, StdoutBytesWritten,
                                 NewLineLast, state);
                ulm_exit((-1, "Abnormal termination \n"));
                break;
            case ALLHOSTSDONE:
                // set flag indicating "app" process can terminate
                ulm_dbg(("client ALLHOSTSDONE arrived on host %d\n",
                         hostIndex));
                Tag = ACKALLHOSTSDONE;
                IOVec.iov_base = (char *) &Tag;
                IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                IOReturn = SendSocket(*ServerSocketFD, 1, &IOVec);
                if (IOReturn < 0) {
                    ulm_exit((-1, "Error: sending ACKALLHOSTSDONE.  "
                              "RetVal: %ld\n", IOReturn));
                }
                /* drain stdio */
                MaxDesc = 0;
                NFDs = ProcessCount[hostIndex] + 1;
                for (i = 0; i < NFDs; i++) {
                    if (STDOUTfdsFromChildren[i] > MaxDesc)
                        MaxDesc = STDOUTfdsFromChildren[i];
                    if (STDERRfdsFromChildren[i] > MaxDesc)
                        MaxDesc = STDERRfdsFromChildren[i];
                }
                MaxDesc++;
                ClientDrainSTDIO(STDOUTfdsFromChildren,
                                 STDERRfdsFromChildren, *ServerSocketFD,
                                 NFDs, MaxDesc, IOPreFix, LenIOPreFix,
                                 StderrBytesWritten, StdoutBytesWritten,
                                 NewLineLast, state);
                *lampiState.sync.AllHostsDone = 1;
                // ok, the client daemon can now exit...
                exit(0);
                break;

            case STDIOMSG:
                if (*STDINfdToChild >= 0)
                    ClientSendStdin(ServerSocketFD, STDINfdToChild);
                break;
            default:
                ulm_exit((-1, "Error: Unrecognized control message (%u)\n",
                          Tag));
            }                   /* end switch */
        }
    }

    return 0;
}
