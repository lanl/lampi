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
#include <stdlib.h>
#include <unistd.h>
#if defined (__linux__) || defined (__APPLE__)
#include <sys/time.h>
#else
#include <time.h>
#endif                          /* LINUX */

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
 * Daemon processing
 */
void runSendHeartbeat(ULMRunParams_t *RunParameters)
{
    unsigned int 	tag;
    int 			errorCode;
	adminMessage	*server;
	
	server = RunParameters->server;
    tag = HEARTBEAT;
	server->reset(adminMessage::SEND);
	
	if ( false == server->broadcastMessage(tag, &errorCode) )
	{
		ulm_err( ("Error: sending HEARTBEAT.  RetVal: %ld\n",
				(long) errorCode) );
	}
}


void MPIrunDaemonize(ssize_t *StderrBytesRead, ssize_t *StdoutBytesRead,
                     ULMRunParams_t *RunParameters)
{
    int ActiveClients;
    int i, MaxDescriptorCtl, MaxDescriptorSTDIO, RetVal;
    double *HeartBeatTime, LastTime, DeltaTime, TimeInSeconds,
        TimeFirstCheckin;
    int *ClientSocketFDList, NHosts, *STDERRfds, *STDOUTfds, *ProcessCnt;
    HostName_t *HostList;
	adminMessage	*server;
#if defined (__linux__) || defined (__APPLE__)
    struct timeval Time;
#else
    struct timespec Time;
#endif                          /* LINUX */
    int *ActiveHosts;           /* if ActiveHost[i] == 1, app still running on that host
                                 *                    -1, Client init not done
                                 *                     0, Client terminated ok */
    pid_t **PIDsOfAppProcs;     /* list of PID's for all Client's children */

    /* Initialization */
    mpirunSetTerminateInitiated(0);
	server = RunParameters->server;
    NHosts = RunParameters->NHosts;
    STDERRfds = RunParameters->STDERRfds;
    STDOUTfds = RunParameters->STDOUTfds;
    ProcessCnt = RunParameters->ProcessCount;
    HostList = RunParameters->HostList;
    ActiveClients = RunParameters->NHosts;
//    /* temporary fix */
//    ClientSocketFDList = (int *) ulm_malloc(sizeof(int)*RunParameters->NHosts);
//    for(i=0 ; i < RunParameters->NHosts ; i++ ) {
//	    ClientSocketFDList[i]=server->clientRank2FD(i);
//    }
//    RunParameters->Networks.TCPAdminstrativeNetwork.SocketsToClients=
//	    ClientSocketFDList;
    ClientSocketFDList = 
	    RunParameters->Networks.TCPAdminstrativeNetwork.SocketsToClients;
//    /* end temporary fix */

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
#if defined (__linux__) || defined (__APPLE__)
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
    MaxDescriptorSTDIO = 0;
    MaxDescriptorCtl = 0;
    for (i = 0; i < NHosts; i++) {
        if (ClientSocketFDList[i] > MaxDescriptorCtl)
            MaxDescriptorCtl = ClientSocketFDList[i];
	if( RunParameters->handleSTDio && STDERRfds ) {
	       	if (STDERRfds[i] > MaxDescriptorSTDIO)
		   	MaxDescriptorSTDIO = STDERRfds[i];
		if (STDOUTfds[i] > MaxDescriptorSTDIO)
		   	MaxDescriptorSTDIO = STDOUTfds[i];
	}
    }
    MaxDescriptorCtl++;
    MaxDescriptorSTDIO++;

    HostsNormalTerminated = 0;
    HostsAbNormalTerminated = 0;
    /* loop over work */
    for (;;) {

        /* check to see if there is any stdout/stderr data to read and then write */
	    if( RunParameters->handleSTDio ) {
		    RetVal = mpirunScanStdErrAndOut(STDERRfds, STDOUTfds, NHosts,
			     	    MaxDescriptorSTDIO, StderrBytesRead, StdoutBytesRead);
	    }

        /* send heartbeat */
#if defined (__linux__) || defined (__APPLE__)
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
			
#ifdef USE_CT
			runSendHeartbeat(RunParameters);
#else
            RetVal =
                _ulm_SendHeartBeat(ClientSocketFDList, NHosts,
                                   MaxDescriptorCtl);
#endif

        }

        /* check if any messages have arrived and process */
#ifdef USE_CT
		RetVal = mpirunCheckForDaemonMsgs(NHosts, HeartBeatTime,
                                      &HostsNormalTerminated,
                                      StderrBytesRead, StdoutBytesRead,
                                      STDERRfds, STDOUTfds,
                                      &HostsAbNormalTerminated,
                                      ActiveHosts, ProcessCnt,
                                      PIDsOfAppProcs,
                                      &ActiveClients, server);
#else
        RetVal =
            mpirunCheckForControlMsgs(MaxDescriptorCtl, ClientSocketFDList,
                                      NHosts, HeartBeatTime,
                                      &HostsNormalTerminated,
                                      StderrBytesRead, StdoutBytesRead,
                                      STDERRfds, STDOUTfds,
                                      &HostsAbNormalTerminated,
                                      ActiveHosts, ProcessCnt,
                                      PIDsOfAppProcs, &TimeFirstCheckin,
                                      &ActiveClients);
#endif

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
#ifdef USE_CT
			RetVal = mpirunCheckForDaemonMsgs(NHosts, HeartBeatTime,
                                      &HostsNormalTerminated,
                                      StderrBytesRead, StdoutBytesRead,
                                      STDERRfds, STDOUTfds,
                                      &HostsAbNormalTerminated,
                                      ActiveHosts, ProcessCnt,
                                      PIDsOfAppProcs,
                                      &ActiveClients, server);
#else
            RetVal =
                mpirunCheckForControlMsgs(MaxDescriptorCtl,
                                          ClientSocketFDList, NHosts,
                                          HeartBeatTime,
                                          &HostsNormalTerminated,
                                          StderrBytesRead, StdoutBytesRead,
                                          STDERRfds, STDOUTfds,
                                          &HostsAbNormalTerminated,
                                          ActiveHosts, ProcessCnt,
                                          PIDsOfAppProcs,
                                          &TimeFirstCheckin,
                                          &ActiveClients);
#endif
            // Terminate all hosts
            Abort();
            ulm_dbg(("Abnormal termination. HostsAbNormalTerminated = %d\n", HostsAbNormalTerminated));
            exit(15);
        }

        /* check to see if any hosts have timed out - the first
         * host to time out it the return value of mpirunCheckHeartBeat
         */
        if (DeltaTime >= HEARTBEATINTERVAL) {
            RetVal =
                mpirunCheckHeartBeat(HeartBeatTime, TimeInSeconds, NHosts,
                                     ActiveHosts,
                                     RunParameters->HeartBeatTimeOut);

            if (RetVal < NHosts) {
                /* send TERMINATENOW message to remaining hosts */
                ulm_err(("Error: Host %d is no longer participating in the job\n", RetVal));
                ClientSocketFDList[RetVal] = -1;
                mpirunKillAppProcs(HostList[RetVal], ProcessCnt[RetVal],
                                   PIDsOfAppProcs[RetVal]);
                HostsAbNormalTerminated++;
                HostsAbNormalTerminated +=
                    mpirunAbortAllHosts(ClientSocketFDList, NHosts, server);
            }
        }

        /* check to see if all clients have started up - if not and
         *  timeout period expired - abort */
        if (TimeFirstCheckin > 0
            && TimeInSeconds - TimeFirstCheckin > STARTUPINTERVAL) {
            ulm_err(("Error: mpirun terminating abnormally\n"));
            printf("Clients did not startup.  List:");
            for (i = 0; i < NHosts; i++)
                if (ActiveHosts[i] == -1)
                    printf(" %d", i);
            printf("\n");
            Abort();
        }
    }                           /* end for loop */
}
