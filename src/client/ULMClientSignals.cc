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
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <signal.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "internal/log.h"
#include "client/ULMClient.h"
#include "queue/globals.h"
#include "client/SocketGeneric.h"
#include "queue/globals.h"
#include "util/dclock.h"

/*
 * Handle Client's SIGCHLD
 */
void Clientsig_child(int Signal)
{
    int i, Status;
    pid_t PIDofChild;

    enum { DEBUG = 0 };

    ulm_dbg(("Signal %d came %f\n", Signal, dclock()));

    switch (Signal) {
    case SIGINT:
        /*
         * ULMrun also receives SIGINT and initiates cleanup
         * -- ignore for now
         */
        break;
    case SIGILL:
    case SIGABRT:
    case SIGFPE:
    case SIGSEGV:
    case SIGPIPE:
    case SIGBUS:
    case SIGHUP:
    case SIGQUIT:
    case SIGTRAP:
    case SIGSYS:
    case SIGALRM:
    case SIGURG:
        ulm_dbg(("Error: Signal %d received. Exiting...\n", Signal));
        exit(3);
        break;

    case SIGCHLD:
        /* return if sigchld already caught */
        if (lampiState.AbnormalExit->flag > 0)
            return;

        /* process signal -
         *  want to see if this is a normal termination, or
         *  abnormal termination
         */

        do {
            errno = 0;
            PIDofChild = waitpid(-1, &Status, WNOHANG);
        } while (PIDofChild <= 0 && errno == EINTR);

        /* normal termination */
        if (PIDofChild == -1 ||
            !(WIFSIGNALED(Status) ||
              (WIFEXITED(Status) && WEXITSTATUS(Status)))) {
            return;
        }

        /*abnormal termination */
        
        /*
         * if my parent is not the Client - exit : this will propagate
         * the SIGCHLD
         */
        if (getpid() != lampiState.PIDofClientDaemon) {
            exit(1);
        }

        /* figure out which process terminated abnormally */
        lampiState.AbnormalExit->pid = -1;
        for (i = 0; i < local_nprocs(); i++) {
            if (PIDofChild == lampiState.ChildPIDs[i]) {
                ulm_dbg(("YourPID: %d, Child PID: %d, i: %d\n",
                         getpid(), lampiState.ChildPIDs[i], i));
                /* save PID of terminated Child to global variable */
                lampiState.AbnormalExit->pid = PIDofChild;
            }
        }
        if (lampiState.AbnormalExit->pid == -1) {
            return;
        }
        /* set global flag indicating abnormal termination */
        lampiState.AbnormalExit->flag = 1;
        lampiState.AbnormalExit->signal = (WIFSIGNALED(Status)) ? WTERMSIG(Status) : 0;
        lampiState.AbnormalExit->status = (WIFEXITED(Status)) ? WEXITSTATUS(Status) : 0;

        ulm_dbg(("child %d exited, signal %d exit status %d\n",
                 (int) lampiState.AbnormalExit->pid,
                 (int) lampiState.AbnormalExit->signal,
                 (int) lampiState.AbnormalExit->status));
        break;
    default:
        break;
    }                           /* end switch */
}


/*
 * terminate all the worker process after Client caught a sigchld
 * from a abnormally terminating child process with pid PIDofChild
 */
void ClientAbnormalChildTermination(pid_t PIDofChild, int NChildren,
                                    pid_t *ChildPIDs, int *IAmAlive,
                                    int SocketToULMRun)
{
    int i, j, TerminatedLocalProcess = 0, TerminatedGlobalProcess = 0;
    unsigned int ErrorData[5], Tag;
    ulm_iovec_t IOVec[2];
    ssize_t IOReturn;

    enum { DEBUG = 0 };

    /* figure out which process terminated abnormally */
    for (i = 0; i < NChildren; i++) {
        if (PIDofChild == ChildPIDs[i]) {
            TerminatedLocalProcess = i;
            TerminatedGlobalProcess = i;
            IAmAlive[TerminatedLocalProcess] = -1;
        }
    }
    ChildPIDs[TerminatedLocalProcess] = -1;
    for (j = 0; j < lampiState.hostid; j++)
        TerminatedGlobalProcess += lampiState.map_host_to_local_size[j];
    ulm_dbg(("About to send SIGKILL\n"));

    for (j = 0; j < NChildren; j++) {
        if (ChildPIDs[j] != -1) {
            kill(ChildPIDs[j], SIGKILL);
            ChildPIDs[j] = -1;
        }
        IAmAlive[j] = -1;
    }

    /* send mpirun notice of abnormal termination */
    if (DEBUG) {
        ulm_err(("Error: AAAAAA ABNORMALTERM being sent\n"));
    }
    Tag = ABNORMALTERM;
    ErrorData[0] = (unsigned int) PIDofChild;
    ErrorData[1] = (unsigned int) TerminatedLocalProcess;
    ErrorData[2] = (unsigned int) TerminatedGlobalProcess;
    ErrorData[3] = (unsigned int) lampiState.AbnormalExit->signal;
    ErrorData[4] = (unsigned int) lampiState.AbnormalExit->status;
    IOVec[0].iov_base = (char *) &Tag;
    IOVec[0].iov_len = (ssize_t) (sizeof(unsigned int));
    IOVec[1].iov_base = (char *) ErrorData;
    IOVec[1].iov_len = (ssize_t) (5 * sizeof(unsigned int));
    IOReturn = _ulm_Send_Socket(SocketToULMRun, 2, IOVec);
    if (IOReturn < 0) {
        ulm_exit((-1, "Error: sending ABNORMALTERM.  RetVal: %ld\n",
                  IOReturn));
    }
}


/*
 * terminate all the worker process after Client caught a sigchld
 * from a abnormally terminating child process with pid PIDofChild
 */
void daemonAbnormalChildTermination(pid_t PIDofChild, int NChildren,
                                    pid_t *ChildPIDs, int *IAmAlive,
                                    lampiState_t *s)
{
    int 			i, j, TerminatedLocalProcess = 0, TerminatedGlobalProcess = 0;
    unsigned int 	ErrorData[5], Tag;
	int				errorCode;
	
    enum { DEBUG = 0 };

    /* figure out which process terminated abnormally */
    for (i = 0; i < NChildren; i++) {
        if (PIDofChild == ChildPIDs[i]) {
            TerminatedLocalProcess = i;
            TerminatedGlobalProcess = i;
            IAmAlive[TerminatedLocalProcess] = -1;
        }
    }
    ChildPIDs[TerminatedLocalProcess] = -1;
    for (j = 0; j < s->hostid; j++)
        TerminatedGlobalProcess += s->map_host_to_local_size[j];
    ulm_dbg(("About to send SIGKILL\n"));

    for (j = 0; j < NChildren; j++) {
        if (ChildPIDs[j] != -1) {
            kill(ChildPIDs[j], SIGKILL);
            ChildPIDs[j] = -1;
        }
        IAmAlive[j] = -1;
    }

    /* send mpirun notice of abnormal termination */
    if (DEBUG) {
        ulm_err(("Error: AAAAAA ABNORMALTERM being sent\n"));
    }
    Tag = ABNORMALTERM;
    ErrorData[0] = (unsigned int) PIDofChild;
    ErrorData[1] = (unsigned int) TerminatedLocalProcess;
    ErrorData[2] = (unsigned int) TerminatedGlobalProcess;
    ErrorData[3] = (unsigned int) lampiState.AbnormalExit->signal;
    ErrorData[4] = (unsigned int) lampiState.AbnormalExit->status;

	s->client->reset(adminMessage::SEND);
	if ( false == s->client->pack(ErrorData, adminMessage::INTEGER, 5)  )
	{
        ulm_exit((-1, "Error: packing msg for sending ABNORMALTERM.  RetVal: %ld\n",
				(long) errorCode) );
	}

	if ( false == s->client->sendMessage(0, Tag, s->channelID, &errorCode) )
	{
        ulm_exit((-1, "Error: sending ABNORMALTERM.  RetVal: %ld\n",
				(long) errorCode) );
	}
}



