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
/* debug */
#include <errno.h>
/* end debug*/

#include "client/SocketGeneric.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/TV.h"
#include "ulm/errors.h"

/*
 * This routine is used to read control messages send to mpirun
 *  from the Client's.
 */
static int NumHostPIDsRecved = 0;

							  
int CheckForControlMsgs(int MaxDescriptor, int *ClientSocketFDList,
                        int NHosts, double *HeartBeatTime,
                        int *HostsNormalTerminated,
                        ssize_t * StderrBytesRead,
                        ssize_t * StdoutBytesRead, 
                        int *HostsAbNormalTerminated,
                        int *ActiveHosts, int *ProcessCnt,
                        pid_t ** PIDsOfAppProcs,
                        double *TimeFirstCheckin, int *ActiveClients)
{
    ulm_fd_set_t ReadSet;
    int i, j, RetVal, error;
    ssize_t ExpetctedData[2], IOReturn;
    unsigned int *Inp, Tag;
    struct timeval WaitTime;
    char ReadBuffer[ULM_MAX_CONF_FILELINE_LEN];
    ulm_iovec_t IOVec;
#ifndef HAVE_CLOCK_GETTIME
    struct timeval Time;
#else
    struct timespec Time;
#endif
#ifdef PURIFY
    ExpetctedData[0] = 0;
    ExpetctedData[1] = 0;
#endif

    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 100000;


    bzero(&ReadSet, sizeof(ReadSet));
    for (i = 0; i < NHosts; i++)
        if (ClientSocketFDList[i] > 0) {
            FD_SET(ClientSocketFDList[i], (fd_set *)&ReadSet);
        }

    RetVal = select(MaxDescriptor, (fd_set *)&ReadSet, NULL, NULL, &WaitTime);
    if (RetVal < 0)
        return RetVal;
    if (RetVal > 0) {
        for (i = 0; i < NHosts; i++) {
            if ((ClientSocketFDList[i] > 0)
                && (FD_ISSET(ClientSocketFDList[i], (fd_set *)&ReadSet))) {
                /* Read tag value */
#ifdef PURIFY
                Tag = 0;
                error = 0;
#endif
                IOReturn = RecvSocket(ClientSocketFDList[i], &Tag,
                                      sizeof(unsigned int), &error);
                /* one end of pipe closed */
                if (IOReturn == 0 || error != ULM_SUCCESS) {
                    ulm_dbg(("IOReturn = 0 host %d, error = %d\n", i,
                             error));
                    ClientSocketFDList[i] = -1;
                    if (ActiveHosts[i])
                        (*HostsAbNormalTerminated)++;
                    else
                        (*ActiveClients)--;
                    continue;
                }
                if (IOReturn < 0) {
                    ulm_err(("Error: Reading tag (%ld)\n", (long) IOReturn));
                    Abort();
                }
                switch (Tag) {  /* process according to message type */
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
                    ExpetctedData[0] = 0;
                    ExpetctedData[1] = 0;
                    error = 0;
#endif
                    IOReturn =
                        RecvSocket(ClientSocketFDList[i],
                                   ExpetctedData,
                                   2 * sizeof(ssize_t), &error);
                    if (IOReturn < 0 || error != ULM_SUCCESS) {
                        ulm_err(("Error: Reading ExpectedData. "
                                 "RetVal = %ld, error = %d\n",
                                 (long) IOReturn, error));
                        Abort();
                    }

                    Tag = ACKNORMALTERM;
                    IOVec.iov_base = (char *) &Tag;
                    IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                    /* send ack */
                    IOReturn =
                        SendSocket(ClientSocketFDList[i], 1, &IOVec);
                    if (IOReturn < 0) {
                        ulm_err(("Error: sending ACKNORMALTERM. RetVal: %ld\n",
                                 (long) IOReturn));
                        Abort();
                    }
                    break;
                case ACKACKNORMALTERM:
                    ulm_dbg(("ACKACKNORMALTERM host %d\n", i));
                    if (ActiveHosts[i])
                        (*HostsNormalTerminated)++;
                    ActiveHosts[i] = 0;
                    // if all hosts have terminated normally
                    //  notify all hosts, so that they can stop network processing
                    //  and shut down.
                    if ((*HostsNormalTerminated) == NHosts) {
                        Tag = ALLHOSTSDONE;
                        ulm_dbg(("ALLHOSTSDONE in mpirun\n"));
                        IOVec.iov_base = (char *) &Tag;
                        IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                        for (j = 0; j < NHosts; j++) {
                            /* send request if socket still open */
                            if (ClientSocketFDList[j] > 0) {
                                ulm_dbg(("sending ALLHOSTSDONE to host %d\n", j));
                                IOReturn =
                                    SendSocket(ClientSocketFDList[j],
                                               1, &IOVec);
                                /* if IOReturn <= 0 assume connection no longer available */
                                if (IOReturn <= 0) {
                                    if (ActiveHosts[i])
                                        (*HostsAbNormalTerminated)++;
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
                case CONFIRMABORTCHILDPROC:
                    break;
                    /* worker process died abnormally */
                case ABNORMALTERM:
#ifdef PURIFY
                    bzero(ReadBuffer, 5 * sizeof(unsigned int));
                    error = 0;
#endif
                    IOReturn =
                        RecvSocket(ClientSocketFDList[i], ReadBuffer,
                                   5 * sizeof(unsigned int), &error);
                    if (IOReturn > 0 || error != ULM_SUCCESS) {
                        Inp = (unsigned int *) ReadBuffer;
                        ulm_err(("Abnormal termination: "
                                 "Global Rank %u Local Host Rank %u PID %u "
                                 "HostID %d Signal %d ExitStatus %d\n",
                                 *(Inp + 2), *(Inp + 1), *Inp, i, *(Inp + 3),
                                 *(Inp + 4)));
                    }
                    /* send ack to Client */
                    Tag = ACKABNORMALTERM;
                    IOVec.iov_base = (char *) &Tag;
                    IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                    IOReturn =
                        SendSocket(ClientSocketFDList[i], 1, &IOVec);
                    if (IOReturn <= 0) {
                        /* assume connection no longer available */
                        ulm_dbg((" ACKABNORMALTERM IOReturn <= 0 host %d\n", i));
                        if (ActiveHosts[i])
                            (*HostsAbNormalTerminated)++;
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                    }
                    /* notify all other clients to terminate immediately */
                    Tag = TERMINATENOW;
                    IOVec.iov_base = (char *) &Tag;
                    IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
                    for (j = 0; j < NHosts; j++) {
                        /* send request if socket still open */
                        if ((j != i) && (ClientSocketFDList[j] > 0)) {
                            IOReturn =
                                SendSocket(ClientSocketFDList[j], 1,
                                           &IOVec);
                            /* if IOReturn <= 0 assume connection no longer available */
                            if (IOReturn <= 0) {
                                if (ActiveHosts[i])
                                    (*HostsAbNormalTerminated)++;
                                ActiveHosts[i] = 0;
                            }
                        }
                    }
                    break;
                case ACKTERMINATENOW:
                    if (ActiveHosts[i])
                        (*HostsAbNormalTerminated)++;
                    ClientSocketFDList[i] = -1;
                    ActiveHosts[i] = 0;
                    (*ActiveClients)--;
                    break;
                case ACKACKABNORMALTERM:
                    if (ActiveHosts[i])
                        (*HostsAbNormalTerminated)++;
                    ClientSocketFDList[i] = -1;
                    ActiveHosts[i] = 0;
                    (*ActiveClients)--;
                    break;
                case APPPIDS:
                    /* read in app process pid's */
#ifdef PURIFY
                    bzero(PIDsOfAppProcs[i],
                          ProcessCnt[i] * sizeof(pid_t));
                    error = 0;
#endif
                    IOReturn =
                        RecvSocket(ClientSocketFDList[i],
                                   PIDsOfAppProcs[i],
                                   ProcessCnt[i] * sizeof(pid_t),
                                   &error);
                    if (IOReturn !=
                        (ssize_t) (ProcessCnt[i] * sizeof(pid_t))
                        || error != ULM_SUCCESS) {
                        ulm_err(("Error: Wrong number of PID's received for host %d.\n "
                                 "\t%ld bytes received,  %ld expected., error = %d\n",
                                 i, (long) IOReturn,
                                 (long) (ProcessCnt[i] * sizeof(pid_t)),
                                 error));
                        Abort();
                    }
                    NumHostPIDsRecved++;
                    /* debug setup - if need be */
                    if (NumHostPIDsRecved == NHosts)
                        MPIrunTVSetUpApp(PIDsOfAppProcs);
                    break;
                case STDIOMSG:
                {
                    int StdioFD;
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          &StdioFD,
                                          sizeof(StdioFD),
                                          &error);
                    if (IOReturn != sizeof(StdioFD) || error != ULM_SUCCESS) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        break;
                    }

                    int bytes;
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          &bytes,
                                          sizeof(bytes),
                                          &error);
                    if (IOReturn != sizeof(bytes) || error != ULM_SUCCESS || bytes == 0) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        break;
                    }
                    
                    char *buff = (char*)malloc(bytes);
                    IOReturn = RecvSocket(ClientSocketFDList[i],
                                          buff,
                                          bytes,
                                          &error);
                    if (IOReturn != bytes || error != ULM_SUCCESS) {
                        ClientSocketFDList[i] = -1;
                        ActiveHosts[i] = 0;
                        free(buff);
                        break;
                    }
                    write(StdioFD, buff, bytes);
                    free(buff);
                    break;
                }
                default:
                    ulm_err(("Error: Unrecognized control message : %d ", Tag));
                    Abort();
                }               /* end switch */
            }
        }                       /* loop over hosts (i) */
    }

    return 0;
}
