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
#include "run/RunParams.h"

int StdInCTS = true;

/*
 * Read from stdin and create an admin message to host rank 0.
 */
static int ScanStdIn(int fdin)
{
    static char buff[ULM_MAX_IO_BUFFER];
    int rc = ULM_SUCCESS;

    if (StdInCTS == false) {
        return ULM_SUCCESS;
    }

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

    if (RunParams.Verbose) {
        ulm_err(("stdin: sending %d bytes to daemon 0\n", cnt));
    }

    adminMessage *server = RunParams.server;
    server->reset(adminMessage::SEND);
    server->pack(&cnt, adminMessage::INTEGER, 1);
    if (cnt > 0) {
        StdInCTS = false;
        server->pack(buff, adminMessage::BYTE, cnt);
    }

    if (false == server->send(0, STDIOMSG, &rc)) {
        ulm_err(("ScanStdIn: error sending STDIOMSG: errno=%d\n", rc));
        return ULM_ERROR;
    }
    return rc;
}


void RunEventLoop(void)
{
    double heartbeat_time;
    double time;
    int rc;
    int *fd;

    fd = RunParams.Networks.TCPAdminstrativeNetwork.SocketsToClients;
    heartbeat_time = ulm_time();
    RunParams.HostsNormalTerminated = 0;
    RunParams.HostsAbNormalTerminated = 0;

    /* setup list of active hosts */
    RunParams.ActiveHost = ulm_new(int, RunParams.NHosts);
    for (int i = 0; i < RunParams.NHosts; i++) {
        RunParams.ActiveHost[i] = 1;
    }

    /* setup stdin file descriptor to be non-blocking */
    if (RunParams.STDINfd >= 0) {
        if (isatty(RunParams.STDINfd)) {    /* input from terminal */
            struct termios term;
            if (tcgetattr(RunParams.STDINfd, &term) != 0) {
                ulm_err(("tcgetattr(%d) failed with errno=%d\n",
                         RunParams.STDINfd, errno));
                RunParams.STDINfd = -1;
            }
            term.c_lflag &= ~ICANON;
            term.c_cc[VMIN] = 0;
            term.c_cc[VTIME] = 0;
            if (tcsetattr(RunParams.STDINfd, TCSANOW, &term) != 0) {
                ulm_err(("tcsetattr(%d) failed with errno=%d\n",
                         RunParams.STDINfd, errno));
                RunParams.STDINfd = -1;
            }
        } else {                            /* input from pipe or file */
            struct stat sbuf;
            if (fstat(RunParams.STDINfd, &sbuf) != 0) {
                ulm_err(("stat(%d) failed with errno=%d\n",
                         RunParams.STDINfd, errno));
            } else if (S_ISFIFO(sbuf.st_mode)) {
                int flags;
                if (fcntl(RunParams.STDINfd, F_GETFL, &flags) != 0) {
                    ulm_err(("fcntl(%d,F_GETFL) failed with errno=%d\n",
                             RunParams.STDINfd, errno));
                    RunParams.STDINfd = -1;
                }
                flags |= O_NONBLOCK;
                if (fcntl(RunParams.STDINfd, F_SETFL, flags) != 0) {
                    ulm_err(("fcntl(%d,F_SETFL,O_NONBLOCK) "
                             "failed with errno=%d\n",
                             RunParams.STDINfd, errno));
                    RunParams.STDINfd = -1;
                }
            }
        }
    }

    /* main event loop */

    while (RunParams.HostsNormalTerminated != RunParams.NHosts) {

        /* is there any stdin data to forward? */
        if (RunParams.handleSTDio && RunParams.STDINfd >= 0) {
            if (ScanStdIn(RunParams.STDINfd) != ULM_SUCCESS) {
                close(RunParams.STDINfd);
                RunParams.STDINfd = -1;
            }
        }

        /* check if any messages have arrived and process */
        rc = CheckForControlMsgs();
        if (rc < 0 || RunParams.HostsAbNormalTerminated > 0) {
            /* last check if any messages have arrived and process */
            CheckForControlMsgs();
            Abort();
        }

        /* send keep-alive heartbeat */
        time = ulm_time();
        if ((time - heartbeat_time) > (double) RunParams.HeartbeatPeriod) {
            heartbeat_time = time;
            SendHeartBeat(fd, RunParams.NHosts);
        }
    }                           /* end for loop */
}
