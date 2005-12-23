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
#include <stdlib.h>

#include "internal/profiler.h"
#include "client/SocketGeneric.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"

int AbnormalExitStatus = 0;

static int TerminateInitiated = 0;

/*
 * abort a user application
 */
void AbortFunction(const char *file, int line)
{
    int *fd = RunParams.Networks.TCPAdminstrativeNetwork.SocketsToClients;
    int tag;
    ulm_iovec_t iovec;

    if (!RunParams.ClientsSpawned) {

        _ulm_set_file_line(file, line);
        _ulm_log("mpirun exiting: no clients spawned\n");

        if (RunParams.CmdLineOK == 0) {
            AbnormalExitStatus = MPIRUN_EXIT_INVALID_ARGUMENTS;
            Usage(stderr);
        }

    } else if (TerminateInitiated == 0) {

        _ulm_set_file_line(file, line);
        _ulm_log("mpirun exiting: aborting clients\n");

        if (!fd) {
            exit(AbnormalExitStatus);
        }

        if (ENABLE_RMS) {
            exit(AbnormalExitStatus);
        }

        TerminateInitiated = 1;

        /* send abort message to each host */
        tag = TERMINATENOW;
        iovec.iov_base = (char *) &tag;
        iovec.iov_len = (ssize_t) sizeof(int);
        if (fd) {
            for (int i = 0; i < RunParams.NHosts; i++) {
                /* send only to hosts that are still alive */
                if (fd[i] > 0) {
                    if (SendSocket(fd[i], 1, &iovec) <= 0) {
                        /* with failed send, register host as down */
                        close(fd[i]);
                        fd[i] = -1;
                        RunParams.HostsAbNormalTerminated++;
                    }
                }
            }
        }

        /* one last attempt to clean up errant processes */
        for (int h = 0; h < RunParams.NHosts; h++) {
            if (RunParams.ActiveHost[h]) {
                KillAppProcs(h);
            }
        }
    }

    LogJobAbort();
    exit(AbnormalExitStatus);
}
