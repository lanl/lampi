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
#include <unistd.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <time.h>

#include "init/fork_many.h"
#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "queue/globals.h"
#include "client/SocketGeneric.h"
#include "util/Utility.h"

static void CleanupOnAbnormalChildTermination(pid_t, int, pid_t *, int *, int);

/*
 * this is code executed by the parent that becomes the Client daemon
 */

void lampi_daemon_loop(lampiState_t *s)
{
    int *ProcessCount = s->map_host_to_local_size;
    int hostIndex = s->hostid;
    int ServerSocketFD = s->client->socketToServer_m;
    pid_t *ChildPIDs = (pid_t *) s->local_pids;
    int *STDINfdToChild = &s->STDINfdToChild;
    int *STDOUTfdsFromChildren = s->STDOUTfdsFromChildren;
    int *STDERRfdsFromChildren = s->STDERRfdsFromChildren;
    PrefixName_t *IOPreFix = s->IOPreFix;
    int *LenIOPreFix = s->LenIOPreFix;
    size_t *StderrBytesWritten = &(s->StderrBytesWritten);
    size_t *StdoutBytesWritten = &(s->StdoutBytesWritten);
    int *NewLineLast = s->NewLineLast;
    int *IAmAlive = s->IAmAlive;

    /* end tmp */
    int i, NumAlive = 0, MaxDescriptor;
    double LastTime = 0.0;
    double HeartBeatTime = 0.0;
    double DeltaTime = 0.0;
    double TimeInSeconds = 0.0;
    unsigned int Message = TERMINATENOW;
    unsigned int NotifyServer;
    bool shuttingDown = false;
#ifndef HAVE_CLOCK_GETTIME
    struct timeval Time;
#else
    struct timespec Time;
#endif                          /* LINUX */
    int NChildren, numDaemonChildren;

    /* initialize data */
    NChildren = ProcessCount[hostIndex];

    /* change heartbeat timeout, if being debugged */
    if (s->started_under_debugger) {
        s->doHeartbeat = 0;
    }

    if (s->doHeartbeat) {
        LastTime = 0.0;
#ifndef HAVE_CLOCK_GETTIME
        gettimeofday(&Time, NULL);
        HeartBeatTime = (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
        clock_gettime(CLOCK_REALTIME, &Time);
        HeartBeatTime = (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
    }

    MaxDescriptor = 0;
    for (i = 0; i < (NChildren + 1); i++) {
        if (STDOUTfdsFromChildren[i] > MaxDescriptor)
            MaxDescriptor = STDOUTfdsFromChildren[i];
        if (STDERRfdsFromChildren[i] > MaxDescriptor)
            MaxDescriptor = STDERRfdsFromChildren[i];
    }
    if (s->commonAlivePipe[0] > MaxDescriptor) {
        MaxDescriptor = s->commonAlivePipe[0];
    }
    MaxDescriptor++;

    numDaemonChildren = 0;
    for (;;) {

        if (s->doHeartbeat) {
#ifndef HAVE_CLOCK_GETTIME
            gettimeofday(&Time, NULL);
            TimeInSeconds = (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
            clock_gettime(CLOCK_REALTIME, &Time);
            TimeInSeconds = (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
            DeltaTime = TimeInSeconds - LastTime;
        }

        /* check to see if any children have exited abnormally */
        if (s->AbnormalExit->flag == 1) {
            CleanupOnAbnormalChildTermination(s->AbnormalExit->pid,
                                              NChildren, ChildPIDs, IAmAlive,
                                              ServerSocketFD);
            /*
             * set abnormal termination flag to 2, so that termination
             * sequence does not start up again.
             */
            s->AbnormalExit->flag = 2;
            shuttingDown = true;
        }
        /* handle stdio and check commonAlivePipe */
        ClientScanStdoutStderr(STDOUTfdsFromChildren,
                               STDERRfdsFromChildren,
                               &ServerSocketFD,
                               NChildren + 1, MaxDescriptor, IOPreFix,
                               LenIOPreFix, StderrBytesWritten,
                               StdoutBytesWritten, NewLineLast, s);
        /* handle stdin */
        if(STDINfdToChild >= 0)  {
            ClientSendStdin(&ServerSocketFD, STDINfdToChild);
        }

        /* send heartbeat to Server */
        if (s->doHeartbeat) {
            if (DeltaTime > (double) s->HeartbeatPeriod) {
                SendHeartBeat(&ServerSocketFD, 1, ServerSocketFD + 1);
                LastTime = TimeInSeconds;
            }
        }

        /* check to see if children alive */
        NumAlive = ClientCheckIfChildrenAlive(ProcessCount,
                                              hostIndex, IAmAlive);

        /* cleanup and exit */
        if ((s->AbnormalExit->flag == 0) && (NumAlive == 0)
            && (!shuttingDown)) {
            /* make sure we check our children's exit status as well before continuing */
            daemon_wait_for_children();
            if (s->AbnormalExit->flag == 0) {
                shuttingDown = true;
                ClientOrderlyShutdown(ServerSocketFD);
            }
        }
        /* check to see if any control messages have arrived from the server */
        ClientCheckForControlMsgs(ServerSocketFD + 1, &ServerSocketFD,
                                  &HeartBeatTime, ProcessCount,
                                  hostIndex, ChildPIDs,
                                  STDINfdToChild,
                                  STDOUTfdsFromChildren,
                                  STDERRfdsFromChildren,
                                  StderrBytesWritten,
                                  StdoutBytesWritten, NewLineLast,
                                  IOPreFix, LenIOPreFix, s);

        if (s->doHeartbeat) {
            /* check to see if Server has timed out */
            if ((TimeInSeconds - HeartBeatTime) > s->HeartbeatTimeout
                && (!shuttingDown)) {
                NotifyServer = 0;
                ulm_err(("Error: lost heartbeat from mpirun "
                         "(period = %d, timeout = %d)\n",
                         s->HeartbeatPeriod, s->HeartbeatTimeout));
                ClientAbort(ServerSocketFD, ProcessCount, hostIndex,
                            ChildPIDs, Message, NotifyServer);
            }                    
        }
    }                           /* end for(;;) */
}


/*
 * terminate all the worker process after client caught a sigchld
 * from a abnormally terminating child process with pid PIDofChild
 */
static void CleanupOnAbnormalChildTermination(pid_t PIDofChild,
                                              int NChildren,
                                              pid_t *ChildPIDs,
                                              int *IAmAlive,
                                              int SocketToULMRun)
{
    int TerminatedLocalProcess = 0, TerminatedGlobalProcess = 0;
    unsigned tag;
    ulm_iovec_t iov[2];
    abnormal_term_msg_t abnormal_term_msg;

    /* figure out which process terminated abnormally */
    for (int i = 0; i < NChildren; i++) {
        if (PIDofChild == ChildPIDs[i]) {
            TerminatedLocalProcess = i;
            TerminatedGlobalProcess = i;
            IAmAlive[TerminatedLocalProcess] = -1;
        }
    }
    ChildPIDs[TerminatedLocalProcess] = -1;
    for (int i = 0; i < lampiState.hostid; i++) {
        TerminatedGlobalProcess += lampiState.map_host_to_local_size[i];
    }
    
    if (lampiState.verbose) {
        ulm_err(("Error: Application process exited abnormally\n"));
        ulm_err(("*** Killing child processes\n"));
    }

    for (int i = 0; i < NChildren; i++) {
        if (ChildPIDs[i] != -1) {
            if (lampiState.verbose) {
                ulm_err(("*** kill -SIGKILL %ld\n", (long) ChildPIDs[i]));
            }
            kill(ChildPIDs[i], SIGKILL);
            ChildPIDs[i] = -1;
        }
        IAmAlive[i] = -1;
    }

    /* send mpirun notice of abnormal termination */
    if (lampiState.verbose) {
        ulm_err(("*** ABNORMALTERM being sent\n"));
    }

    tag = ABNORMALTERM;
    abnormal_term_msg.pid    = (unsigned) PIDofChild;                     
    abnormal_term_msg.lrank  = (unsigned) TerminatedLocalProcess;         
    abnormal_term_msg.grank  = (unsigned) TerminatedGlobalProcess;        
    abnormal_term_msg.signal = (unsigned) lampiState.AbnormalExit->signal;
    abnormal_term_msg.status = (unsigned) lampiState.AbnormalExit->status;

    iov[0].iov_base = &tag;
    iov[0].iov_len = (ssize_t) (sizeof(unsigned int));
    iov[1].iov_base = &abnormal_term_msg;
    iov[1].iov_len = sizeof(abnormal_term_msg_t);
    if (SendSocket(SocketToULMRun, 2, iov) < 0) {
        ulm_exit(("Error: sending ABNORMALTERM\n"));
    }
}

