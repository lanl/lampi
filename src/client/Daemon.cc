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
#include "client/daemon.h"
#include "queue/globals.h"
#include "client/SocketGeneric.h"
#include "util/Utility.h"

static void CleanupOnAbnormalChildTermination(lampiState_t *s);

/*
 * this is code executed by the parent that becomes the Client daemon
 */

void lampi_daemon_loop(lampiState_t *s)
{
    int NumAlive = 0;
    double heartbeat_time = 0.0;
    double time = 0.0;
    unsigned int Message = TERMINATENOW;
    unsigned int NotifyServer = 0;
    bool shuttingDown = false;

    heartbeat_time = ulm_time();

    for (;;) {

        /* check to see if any children have exited abnormally */
        if (s->AbnormalExit->flag == 1) {
            CleanupOnAbnormalChildTermination(s);
            /*
             * set abnormal termination flag to 2, so that termination
             * sequence does not start up again.
             */
            s->AbnormalExit->flag = 2;
            shuttingDown = true;
        }

        /* handle stdio and check commonAlivePipe */
        ClientScanStdoutStderr(s);

        /* handle stdin */
        if (s->STDINfdToChild >= 0)  {
            ClientSendStdin(&(s->client->socketToServer_m),
                            &(s->STDINfdToChild));
        }

        /* send heartbeat to Server */
        time = ulm_time();
        if ((time - heartbeat_time) > (double) s->HeartbeatPeriod) {
            SendHeartBeat(&(s->client->socketToServer_m), 1);
            heartbeat_time = time;
        }

        /* check to see if children alive */
        NumAlive = ClientCheckIfChildrenAlive(s);

        /* cleanup and exit */
        if ((s->AbnormalExit->flag == 0) && (NumAlive == 0)
            && (!shuttingDown)) {
            /* make sure we check our children's exit status as well
             * before continuing */
            daemon_wait_for_children();
            if (s->AbnormalExit->flag == 0) {
                shuttingDown = true;
                ClientOrderlyShutdown(s);
            }
        }

        /* check to see if any control messages have arrived from the server */
        ClientCheckForControlMsgs(s);

        /* check server socket is still there, otherwise abort */
        if (s->client->socketToServer_m < 0) {
            ulm_err(("Error: lost connection to mpirun\n"));
            ClientAbort(s, Message, NotifyServer);
        }
    }                           /* end for(;;) */
}

/*
 * terminate all the worker process after client caught a sigchld
 * from a abnormally terminating child process with pid PIDofChild
 */
static void CleanupOnAbnormalChildTermination(lampiState_t *s)
{
    int TerminatedLocalProcess = 0, TerminatedGlobalProcess = 0;
    unsigned tag;
    ulm_iovec_t iov[2];
    abnormal_term_msg_t abnormal_term_msg;

    /* figure out which process terminated abnormally */
    for (int i = 0; i < s->local_size; i++) {
        if (s->AbnormalExit->pid == s->local_pids[i]) {
            TerminatedLocalProcess = i;
            TerminatedGlobalProcess = i;
            s->IAmAlive[i] = -1;
        }
    }
    s->local_pids[TerminatedLocalProcess] = -1;
    for (int i = 0; i < s->hostid; i++) {
        TerminatedGlobalProcess += s->map_host_to_local_size[i];
    }

    if (s->verbose) {
        ulm_err(("Error: Application process exited abnormally\n"));
        ulm_err(("*** Killing child processes\n"));
    }

    for (int i = 0; i < s->local_size; i++) {
        if (s->local_pids[i] != -1) {
            if (s->verbose) {
                ulm_err(("*** kill -SIGKILL %ld\n", (long) s->local_pids[i]));
            }
            kill((pid_t) s->local_pids[i], SIGKILL);
            s->local_pids[i] = -1;
        }
        s->IAmAlive[i] = -1;
    }

    /* send mpirun notice of abnormal termination */
    if (s->verbose) {
        ulm_err(("*** ABNORMALTERM being sent\n"));
    }

    tag = ABNORMALTERM;
    abnormal_term_msg.pid    = (unsigned) s->local_pids;
    abnormal_term_msg.lrank  = (unsigned) TerminatedLocalProcess;
    abnormal_term_msg.grank  = (unsigned) TerminatedGlobalProcess;
    abnormal_term_msg.signal = (unsigned) s->AbnormalExit->signal;
    abnormal_term_msg.status = (unsigned) s->AbnormalExit->status;

    iov[0].iov_base = &tag;
    iov[0].iov_len = (ssize_t) (sizeof(unsigned int));
    iov[1].iov_base = &abnormal_term_msg;
    iov[1].iov_len = sizeof(abnormal_term_msg_t);
    if (SendSocket(s->client->socketToServer_m, 2, iov) < 0) {
        ulm_exit(("Error: sending ABNORMALTERM\n"));
    }
}
