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
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <signal.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "client/SocketGeneric.h"

/*
 * This routine is used to abort all local children and to notify the
 * Server of this event.
 */
void AbortLocalHost(int ServerSocketFD, int *ProcessCount, int hostIndex,
                    pid_t *ChildPIDs, unsigned int MessageType,
                    int Notify)
{
    int i, NumChildren;
    ulm_iovec_t IOVec;
    ssize_t IOReturn;

    /*
     * kill all children  - this signal will not be processed by mpirun's
     * daemon loop
     */
    NumChildren = ProcessCount[hostIndex];
    for (i = 0; i < NumChildren; i++) {
        if (ChildPIDs[i] != -1) {
            kill(ChildPIDs[i], SIGKILL);
            ChildPIDs[i] = -1;
        }
    }

    /* notify server of termination */
    if (Notify) {
        IOVec.iov_base = (char *) &MessageType;
        IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
        IOReturn = _ulm_Send_Socket(ServerSocketFD, 1, &IOVec);
        if (IOReturn < 0) {
            ulm_exit((-1,
                      "Error: reading Tag in AbortLocalHost.  "
                      "RetVal: %ld\n", IOReturn));
        }
    }

    /*  !!!!!!!!!  may want to do some cleanup here first */
    exit(2);
}

void AbortAndDrainLocalHost(int ServerSocketFD, int *ProcessCount, int hostIndex,
                            pid_t *ChildPIDs, unsigned int MessageType,
                            int Notify, int *ClientStdoutFDs, int *ClientStderrFDs,
                            PrefixName_t *IOPrefix,
                            int *LenIOPreFix, size_t *StderrBytesWritten, size_t *StdoutBytesWritten,
                            int *NewLineLast, lampiState_t *state)
{
    int i, NumChildren, MaxDesc, NFDs;
    ulm_iovec_t IOVec;
    ssize_t IOReturn;
    bool again = true;

    /*
     * kill all children  - this signal will not be processed by mpirun's
     * daemon loop
     */
    NumChildren = ProcessCount[hostIndex];
    for (i = 0; i < NumChildren; i++) {
        if (ChildPIDs[i] != -1) {
            kill(ChildPIDs[i], SIGKILL);
            ChildPIDs[i] = -1;
        }
    }

    MaxDesc = 0;
    NFDs = ProcessCount[hostIndex] + 1;

    for (i = 0; i < NFDs; i++) {
        if (ClientStdoutFDs[i] > MaxDesc)
            MaxDesc = ClientStdoutFDs[i];
        if (ClientStderrFDs[i] > MaxDesc)
            MaxDesc = ClientStderrFDs[i];
    }
    MaxDesc++;

    /* drain standard I/O of children */
    if (ServerSocketFD >= 0) {
        while (again) {
            ClientScanStdoutStderr(ClientStdoutFDs, ClientStderrFDs,
                                   &ServerSocketFD, NFDs, MaxDesc,
                                   IOPrefix, LenIOPreFix, StderrBytesWritten, StdoutBytesWritten,
                                   NewLineLast, state);

            again = false;
            /* scan again only if pipes to children are still open; note
             * that we avoid the last set of descriptors since they are
             * the client daemon's....
             */
            for (i = 0; i < (NFDs - 1); i++) {
                if ((ClientStdoutFDs[i] >= 0) || (ClientStderrFDs[i] >= 0)) {
                    again = true;
                    break;
                }
            }
        }
    }

    ClientStdoutFDs[NFDs - 1] = -1;
    ClientStderrFDs[NFDs - 1] = -1;

    /* notify server of termination */
    if (Notify) {
        IOVec.iov_base = (char *) &MessageType;
        IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
        IOReturn = _ulm_Send_Socket(ServerSocketFD, 1, &IOVec);
        if (IOReturn < 0) {
            ulm_exit((-1,
                      "Error: reading Tag in AbortAndDrainLocalHost.  "
                      "RetVal: %ld\n", IOReturn));
        }
    }

    exit(2);
}
