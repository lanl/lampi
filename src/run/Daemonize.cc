/*
 * Copyright 2002-2004. The Regents of the University of California. This material 
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

#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "util/Utility.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/globals.h"

/*
 * Read from stdin and create an admin message to host rank 0.
 */
static int ScanStdIn(int fdin)
{
    char buff[512];
    int rc = ULM_SUCCESS;
    int cnt = read(fdin, buff, sizeof(buff));
    if (cnt == 0) {
        if (isatty(fdin))
            return ULM_SUCCESS;
        rc = ULM_ERROR;         // send 0 byte message then close descriptors
    }

    if (cnt < 0) {
        switch (errno) {
        case EINTR:
        case EAGAIN:
            return ULM_SUCCESS;
        default:
            ulm_err(("mpirunScanStdIn: read(stdin) failed with errno=%d\n",
                     errno));
            return ULM_ERROR;
        }
    }

    adminMessage *server = RunParameters.server;
    server->reset(adminMessage::SEND);
    server->pack(&cnt, adminMessage::INTEGER, 1);
    if (cnt > 0)
        server->pack(buff, adminMessage::BYTE, cnt);

    if (false == server->send(0, STDIOMSG, &rc)) {
        ulm_err(("ScanStdIn: error sending STDIOMSG: errno=%d\n", rc));
        return ULM_ERROR;
    }
    return rc;
}


void Daemonize(ssize_t *StderrBytesRead, ssize_t *StdoutBytesRead,
               ULMRunParams_t *RunParameters)
{
    int ActiveClients;
    int i, MaxDescriptorCtl, RetVal;
    double *HeartBeatTime, LastTime, DeltaTime, TimeInSeconds,
        TimeFirstCheckin;
    int *ClientSocketFDList, NHosts, *ProcessCnt;
    HostName_t *HostList;
    adminMessage *server;

#ifndef HAVE_CLOCK_GETTIME
    struct timeval Time;
#else
    struct timespec Time;
#endif                          /* LINUX */
    int *ActiveHosts;           /* if ActiveHost[i] == 1, app still running
                                 *                    -1, Client init not done
                                 *                     0, Client terminated ok
                                 */
    pid_t **PIDsOfAppProcs;     /* list of PID's for all Client's children */

    /* Initialization */
    SetTerminateInitiated(0);
    server = RunParameters->server;
    NHosts = RunParameters->NHosts;
    ProcessCnt = RunParameters->ProcessCount;
    HostList = RunParameters->HostList;
    ActiveClients = RunParameters->NHosts;
    ClientSocketFDList =
        RunParameters->Networks.TCPAdminstrativeNetwork.SocketsToClients;

    /* setup list of active hosts */
    ActiveHosts = ulm_new(int, NHosts);
    for (i = 0; i < NHosts; i++)
        ActiveHosts[i] = -1;
    TimeFirstCheckin = -1.0;
    /* setup array to hold list of app PID's */
    PIDsOfAppProcs = ulm_new(pid_t *, NHosts);
    for (i = 0; i < NHosts; i++) {
        PIDsOfAppProcs[i] = ulm_new(pid_t, ProcessCnt[i]);
    }
    /* setup initial control data */
    HeartBeatTime = ulm_new(double, NHosts);
#ifndef HAVE_CLOCK_GETTIME
    gettimeofday(&Time, NULL);
    TimeInSeconds =
        (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
    clock_gettime(CLOCK_REALTIME, &Time);
    TimeInSeconds =
        (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif
    LastTime = TimeInSeconds;
    for (i = 0; i < NHosts; i++)
        HeartBeatTime[i] = TimeInSeconds;
    LastTime = 0;

    /* find the largest descriptor - used for select */
    MaxDescriptorCtl = 0;
    for (i = 0; i < NHosts; i++) {
        if (ClientSocketFDList[i] > MaxDescriptorCtl)
            MaxDescriptorCtl = ClientSocketFDList[i];
    }
    MaxDescriptorCtl++;
    HostsNormalTerminated = 0;
    HostsAbNormalTerminated = 0;

    /* setup stdin file descriptor to be non-blocking */
    if (RunParameters->STDINfd >= 0) {
        /* input from terminal */
        if (isatty(RunParameters->STDINfd)) {

            struct termios term;
            if (tcgetattr(RunParameters->STDINfd, &term) != 0) {
                ulm_err(("tcgetattr(%d) failed with errno=%d\n",
                         RunParameters->STDINfd, errno));
                RunParameters->STDINfd = -1;
            }
            term.c_lflag &= ~ICANON;
            term.c_cc[VMIN] = 0;
            term.c_cc[VTIME] = 0;
            if (tcsetattr(RunParameters->STDINfd, TCSANOW, &term) != 0) {
                ulm_err(("tcsetattr(%d) failed with errno=%d\n",
                         RunParameters->STDINfd, errno));
                RunParameters->STDINfd = -1;
            }
            /* input from pipe or file */
        } else {
            struct stat sbuf;
            if (fstat(RunParameters->STDINfd, &sbuf) != 0) {
                ulm_err(("stat(%d) failed with errno=%d\n",
                         RunParameters->STDINfd, errno));
            } else if (S_ISFIFO(sbuf.st_mode)) {
                int flags;
                if (fcntl(RunParameters->STDINfd, F_GETFL, &flags) != 0) {
                    ulm_err(("fcntl(%d,F_GETFL) failed with errno=%d\n",
                             RunParameters->STDINfd, errno));
                    RunParameters->STDINfd = -1;
                }
                flags |= O_NONBLOCK;
                if (fcntl(RunParameters->STDINfd, F_SETFL, flags) != 0) {
                    ulm_err(("fcntl(%d,F_SETFL,O_NONBLOCK) failed with errno=%d\n", RunParameters->STDINfd, errno));
                    RunParameters->STDINfd = -1;
                }
            }
        }
    }

    /* loop over work */
    for (;;) {

        /* check to see if there is any stdin/stdout/stderr data to read and then write */
        if (RunParameters->handleSTDio && RunParameters->STDINfd >= 0) {
            if (ScanStdIn(RunParameters->STDINfd) != ULM_SUCCESS) {
                close(RunParameters->STDINfd);
                RunParameters->STDINfd = -1;
            }
        }

        /* send heartbeat */
#ifndef HAVE_CLOCK_GETTIME
        gettimeofday(&Time, NULL);
        TimeInSeconds =
            (double) (Time.tv_sec) + ((double) Time.tv_usec) * 1e-6;
#else
        clock_gettime(CLOCK_REALTIME, &Time);
        TimeInSeconds =
            (double) (Time.tv_sec) + ((double) Time.tv_nsec) * 1e-9;
#endif
        DeltaTime = TimeInSeconds - LastTime;
        if (DeltaTime >= HEARTBEATINTERVAL) {
            LastTime = TimeInSeconds;

            RetVal = SendHeartBeat(ClientSocketFDList, NHosts,
                                   MaxDescriptorCtl);
        }

        /* check if any messages have arrived and process */
        RetVal =
            CheckForControlMsgs(MaxDescriptorCtl, ClientSocketFDList,
                                NHosts, HeartBeatTime,
                                &HostsNormalTerminated,
                                StderrBytesRead, StdoutBytesRead,
                                &HostsAbNormalTerminated,
                                ActiveHosts, ProcessCnt,
                                PIDsOfAppProcs, &TimeFirstCheckin,
                                &ActiveClients);

        /* exit if all hosts have terminated normally */
        if (!ActiveClients) {
            /* !!!!!!!!  may want to cleanup */
            /* last check if any messages have arrived and process */
            return;
        }
        /* abnormal exit */
        if (((HostsNormalTerminated + HostsAbNormalTerminated) == NHosts)
            && (HostsAbNormalTerminated > 0)) {
            /* last check if any messages have arrived and process */
            RetVal =
                CheckForControlMsgs(MaxDescriptorCtl,
                                    ClientSocketFDList, NHosts,
                                    HeartBeatTime,
                                    &HostsNormalTerminated,
                                    StderrBytesRead, StdoutBytesRead,
                                    &HostsAbNormalTerminated,
                                    ActiveHosts, ProcessCnt,
                                    PIDsOfAppProcs,
                                    &TimeFirstCheckin, &ActiveClients);

            // Terminate all hosts
            ulm_err(("Abnormal termination. HostsAbNormalTerminated = %d\n", HostsAbNormalTerminated));
            Abort();
            exit(EXIT_FAILURE);
        }

        /* check to see if any hosts have timed out - the first
         * host to time out it the return value of CheckHeartBeat
         */
        if (DeltaTime >= HEARTBEATINTERVAL) {
            RetVal =
                CheckHeartBeat(HeartBeatTime, TimeInSeconds, NHosts,
                               ActiveHosts,
                               RunParameters->HeartBeatTimeOut);

            if (RetVal < NHosts) {
                /* send TERMINATENOW message to remaining hosts */
                ulm_err(("Error: Host %d is no longer participating in the job\n", RetVal));
                ClientSocketFDList[RetVal] = -1;
                KillAppProcs(HostList[RetVal], ProcessCnt[RetVal],
                             PIDsOfAppProcs[RetVal]);
                HostsAbNormalTerminated++;
                HostsAbNormalTerminated +=
                    AbortAllHosts(ClientSocketFDList, NHosts, server);
            }
        }

        /* check to see if all clients have started up - if not and
         *  timeout period expired - abort */
        if (TimeFirstCheckin > 0
            && TimeInSeconds - TimeFirstCheckin > STARTUPINTERVAL) {
            ulm_err(("Error:  mpirun terminating abnormally.\n"));
            ulm_err(("  Clients did not startup.  List: "));
            for (i = 0; i < NHosts; i++)
                if (ActiveHosts[i] == -1)
                    ulm_err((" %d ", i));
            ulm_err(("\n"));
            Abort();
        }
    }                           /* end for loop */
}
