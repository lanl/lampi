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
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <errno.h>

#include "client/SocketGeneric.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/TV.h"
#include "ulm/errors.h"

/*
 * Read control messages sent to mpirun from the client daemons
 */
int CheckForControlMsgs(int MaxDescriptor,
                        int *ClientSocketFDList,
                        int NHosts,
                        double *HeartBeatTime,
                        int *HostsNormalTerminated,
                        int *HostsAbNormalTerminated,
                        int *ActiveHosts,
                        int *ProcessCnt,
                        pid_t **PIDsOfAppProcs,
                        double *TimeFirstCheckin,
                        int *ActiveClients)
{
    char *buf;
    char ReadBuffer[ULM_MAX_CONF_FILELINE_LEN];
    int bytes;
    int error;
    int rc;
    int stdio_fd;
    ssize_t ExpectedData[2];
    ssize_t IOReturn;
    struct timeval WaitTime;
    ulm_fd_set_t ReadSet;
    ulm_iovec_t IOVec;
    unsigned int tag;
#ifndef HAVE_CLOCK_GETTIME
    struct timeval Time;
#else
    struct timespec Time;
#endif

#ifdef PURIFY
    ExpectedData[0] = 0;
    ExpectedData[1] = 0;
#endif
    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 100000;

    bzero(&ReadSet, sizeof(ReadSet));
    for (int i = 0; i < NHosts; i++) {
        if (ClientSocketFDList[i] > 0) {
            FD_SET(ClientSocketFDList[i], (fd_set *)&ReadSet);
        }
    }

    rc = select(MaxDescriptor, (fd_set *)&ReadSet, NULL, NULL, &WaitTime);
    if (rc < 0) {
        return rc;
    }

    if (rc > 0) {
        for (int i = 0; i < NHosts; i++) {
            if ((ClientSocketFDList[i] > 0)
                && (FD_ISSET(ClientSocketFDList[i], (fd_set *)&ReadSet))) {
                /* Read tag value */
#ifdef PURIFY
                tag = 0;
                error = 0;
#endif
                IOReturn = RecvSocket(ClientSocketFDList[i], &tag,
                                      sizeof(unsigned int), &error);
                /* one end of pipe closed */
                if (IOReturn == 0 || error != ULM_SUCCESS) {
                    ulm_dbg(("IOReturn = 0 host %d, error = %d\n", i,
                             error));
                    ClientSocketFDList[i] = -1;
                    if (ActiveHosts[i]) {
                        (*HostsAbNormalTerminated)++;
                    } else {
                        (*ActiveClients)--;
                    }
                    continue;
                }
                if (IOReturn < 0) {
                    ulm_err(("Error: Reading tag (%ld)\n", (long) IOReturn));
                    Abort();
                }

                switch (tag) {  /* process according to message type */

                case HEARTBEAT:
#ifndef HAVE_CLOCK_GETTIME
                    gettimeofday(&Time, NULL);
                    HeartBeatTime[i] =
                        (double) (Time.tv_sec) +
                        ((double) Time.tv_usec) * 1e-6;
#else
                    clock_gettime(CLOCK_REALTIME, &Time);
                    HeartBeatTime[i] =
                        (double) (Time.tv_sec) +
                        ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
                    break;

                case NORMALTERM:
                    ulm_dbg((" NormalTerm %d\n", i));
#ifdef PURIFY
                    ExpectedData[0] = 0;
                    ExpectedData[1] = 0;
                    error = 0;
#endif
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          ExpectedData,
                                          2 * sizeof(ssize_t), &error);
                    if (IOReturn < 0 || error != ULM_SUCCESS) {
                        ulm_err(("Error: Reading ExpectedData. "
                                 "rc = %ld, error = %d\n",
                                 (long) IOReturn, error));
                        Abort();
                    }

                    tag = ACKNORMALTERM;
                    IOVec.iov_base = (char *) &tag;
                    IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                    /* send ack */
                    IOReturn = SendSocket(ClientSocketFDList[i], 1, &IOVec);
                    if (IOReturn < 0) {
                        ulm_err(("Error: sending ACKNORMALTERM. rc: %ld\n",
                                 (long) IOReturn));
                        Abort();
                    }
                    break;

                case ACKACKNORMALTERM:
                    ulm_dbg(("ACKACKNORMALTERM host %d\n", i));
                    if (ActiveHosts[i]) {
                        (*HostsNormalTerminated)++;
                    }
                    ActiveHosts[i] = 0;

                    // if all hosts have terminated normally notify
                    // all hosts, so that they can stop network
                    // processing and shut down.

                    if ((*HostsNormalTerminated) == NHosts) {
                        tag = ALLHOSTSDONE;
                        ulm_dbg(("ALLHOSTSDONE in mpirun\n"));
                        IOVec.iov_base = (char *) &tag;
                        IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                        for (int j = 0; j < NHosts; j++) {
                            /* send request if socket still open */
                            if (ClientSocketFDList[j] > 0) {
                                ulm_dbg(("sending ALLHOSTSDONE to %d\n", j));
                                IOReturn = SendSocket(ClientSocketFDList[j],
                                                      1, &IOVec);
                                /* if IOReturn <= 0 assume connection lost */
                                if (IOReturn <= 0) {
                                    if (ActiveHosts[i]) {
                                        (*HostsAbNormalTerminated)++;
                                    }
                                    ActiveHosts[i] = 0;
                                }
                            }
                        }
                    }           // end sending ALLHOSTSDONE
                    break;

                case ACKALLHOSTSDONE:
                    ulm_dbg((" ACKALLHOSTSDONE host %d\n", i));
                    (*ActiveClients)--;
                    ClientSocketFDList[i] = -1;
                    break;

                case ABNORMALTERM:
#ifdef PURIFY
                    bzero(ReadBuffer, 5 * sizeof(unsigned int));
                    error = 0;
#endif
                    IOReturn = RecvSocket(ClientSocketFDList[i], ReadBuffer,
                                          5 * sizeof(unsigned int), &error);
                    if (IOReturn > 0 || error != ULM_SUCCESS) {
                        unsigned int *Inp = (unsigned int *) ReadBuffer;
                        ulm_err(("Abnormal termination: "
                                 "Global Rank %u Local Host Rank %u PID %u "
                                 "HostID %d Signal %d ExitStatus %d\n",
                                 *(Inp + 2), *(Inp + 1), *Inp, i, *(Inp + 3),
                                 *(Inp + 4)));
                    }
                    AbortAllHosts(ClientSocketFDList, NHosts);
                    Abort();
                    break;

                case STDIOMSG:
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          &stdio_fd, sizeof(stdio_fd), &error);
                    if (IOReturn != sizeof(stdio_fd) || error != ULM_SUCCESS) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        break;
                    }

                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          &bytes, sizeof(bytes), &error);
                    if (IOReturn != sizeof(bytes) ||
                        error != ULM_SUCCESS || bytes == 0) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        break;
                    }
                    
                    buf = (char*) malloc(bytes);
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          buf, bytes, &error);
                    if (IOReturn != bytes || error != ULM_SUCCESS) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        free(buf);
                        break;
                    }
                    write(stdio_fd, buf, bytes);
                    free(buf);
                    break;

                case STDIOMSG_CTS:
                    StdInCTS = true;
                    break;
                default:
                    ulm_err(("Error: Unknown control message: %d\b", tag));
                    Abort();
                }               /* end switch */
            }
        }                       /* loop over hosts (i) */
    }

    return 0;
}
