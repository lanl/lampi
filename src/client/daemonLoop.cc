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


#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>
#if defined(__linux__) || defined(__APPLE__) || defined (__CYGWIN__)
#include <sys/time.h>
#else
#include <time.h>
#endif                          /* LINUX */

#include "init/fork_many.h"
#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "queue/globals.h"
#include "client/SocketGeneric.h"
#include "util/Utility.h"

static double HeartBeatTimeOut = (double) HEARTBEATTIMEOUT;

/*
 * this is code executed by the parent that becomes the Client daemon
 */
 
 
 /*
  *             Prototypes
  */

void daemonSendHeartbeat(lampiState_t *s);


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
    double LastTime, HeartBeatTime, DeltaTime, TimeInSeconds;
    unsigned int Message = TERMINATENOW, NotifyServer;
    bool shuttingDown = false;
#if defined(__linux__) || defined(__APPLE__) || defined (__CYGWIN__)
    struct timeval Time;
#else
    struct timespec Time;
#endif                          /* LINUX */
    int NChildren, numDaemonChildren;

    /* initialize data */
    NChildren = ProcessCount[hostIndex];
#if defined(__linux__) || defined(__APPLE__) || defined (__CYGWIN__)
    gettimeofday(&Time, NULL);
    HeartBeatTime =
        (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
    clock_gettime(CLOCK_REALTIME, &Time);
    HeartBeatTime =
        (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
    LastTime = 0;
    MaxDescriptor = 0;
    for (i = 0; i < (NChildren + 1); i++) {
        if (STDOUTfdsFromChildren[i] > MaxDescriptor)
            MaxDescriptor = STDOUTfdsFromChildren[i];
        if (STDERRfdsFromChildren[i] > MaxDescriptor)
            MaxDescriptor = STDERRfdsFromChildren[i];
    }
    MaxDescriptor++;

    /* change heartbeat timeout period, if being debuged */
    if (s->debug == 1)
        HeartBeatTimeOut = -1;

    numDaemonChildren = 0;
    for (;;) {

#if defined(__linux__) || defined(__APPLE__) || defined (__CYGWIN__)
        gettimeofday(&Time, NULL);
        TimeInSeconds =
            (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
        clock_gettime(CLOCK_REALTIME, &Time);
        TimeInSeconds =
            (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif                          /* LINUX */
        DeltaTime = TimeInSeconds - LastTime;
        /* check to see if any children have exited abnormally */
        if (s->AbnormalExit->flag == 1) {
#ifdef ENABLE_CT
            daemonAbnormalChildTermination(s->AbnormalExit->pid,
                                           NChildren, ChildPIDs, IAmAlive,
                                           s);
#else
            ClientAbnormalChildTermination(s->AbnormalExit->pid,
                                           NChildren, ChildPIDs, IAmAlive,
                                           ServerSocketFD);
#endif
            /*
             * set abnormal termination flag to 2, so that termination
             * sequence does not start up again.
             */
            s->AbnormalExit->flag = 2;
            shuttingDown = true;
        }
        /* handle stdio */
        ClientScanStdoutStderr(STDOUTfdsFromChildren,
                               STDERRfdsFromChildren, 
                               &ServerSocketFD, 
                               NChildren + 1, MaxDescriptor, IOPreFix,
                               LenIOPreFix, StderrBytesWritten,
                               StdoutBytesWritten, NewLineLast, s);

        /* send heartbeat to Server */
#ifdef ENABLE_CT
        if ( (DeltaTime > HEARTBEATINTERVAL) && (!shuttingDown) ){
            daemonSendHeartbeat(s);
            LastTime = TimeInSeconds;
        }
#else
        if (DeltaTime > HEARTBEATINTERVAL) {
            _ulm_SendHeartBeat(&ServerSocketFD, 1, ServerSocketFD + 1);
            LastTime = TimeInSeconds;
        }
#endif

        /* check to see if children alive */
        NumAlive = CheckIfChildrenAlive(ProcessCount, hostIndex, IAmAlive);

        /* cleanup and exit */
        if ((s->AbnormalExit->flag == 0) && (NumAlive == 0) && (!shuttingDown)) {
            /* make sure we check our children's exit status as well before continuing */
            daemon_wait_for_children();
            if (s->AbnormalExit->flag == 0) {
                shuttingDown = true;
                ClientOrderlyShutdown(StderrBytesWritten, StdoutBytesWritten,
                                  ServerSocketFD, s);
            }
        }
#ifdef ENABLE_CT
        checkForRunControlMsgs(&HeartBeatTime, ProcessCount, &numDaemonChildren,
                               hostIndex, ChildPIDs,
                               STDOUTfdsFromChildren,
                               STDERRfdsFromChildren, StderrBytesWritten,
                               StdoutBytesWritten, NewLineLast,
                               IOPreFix, LenIOPreFix, s);
#else
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

#endif
        /* check to see if Server has timed out */
        if ((HeartBeatTimeOut > 0) &&
            ((TimeInSeconds - HeartBeatTime) > (double) HeartBeatTimeOut) && (!shuttingDown) )
        {
            NotifyServer = 0;
#ifdef ENABLE_CT
            daemonAbortLocalHost(ProcessCount, hostIndex,
                           ChildPIDs, Message, NotifyServer, s);
#else						   
            AbortLocalHost(ServerSocketFD, ProcessCount, hostIndex,
                           ChildPIDs, Message, NotifyServer);
#endif
        }
    }                           /* end for(;;) */
}


void daemonSendHeartbeat(lampiState_t *s)
{
    unsigned int tag;
    int errorCode;

    tag = HEARTBEAT;
    s->client->reset(adminMessage::SEND);

    if (false == s->client->sendMessage(0, tag, s->channelID, &errorCode)) {
        ulm_err(("Error: sending HEARTBEAT.  RetVal: %ld\n",
                 (long) errorCode));
    }
}


void daemonAbortLocalHost(int *ProcessCount, int hostIndex,
                          pid_t * ChildPIDs, unsigned int MessageType,
                          int Notify, lampiState_t *s)
{
    int errorCode, i, NumChildren;

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
        s->client->reset(adminMessage::SEND);
        if (false ==
            s->client->sendMessage(0, MessageType, s->channelID,
                                   &errorCode)) {
            ulm_exit((-1,
                      "Error: reading Tag in AbortLocalHost.  "
                      "RetVal: %ld\n", errorCode));
        }
    }

    /*  !!!!!!!!!  may want to do some cleanup here first */
    exit(2);
}
