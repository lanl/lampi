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
#include <sys/types.h>
#include <unistd.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/types.h"
#include "run/Run.h"

/*
 * Kill application processes.
 *
 * Resource manager dependent:
 * rsh, lsf - kill applications on a given host
 * rms - do nothing
 * local, bproc - kill all applications
 *
 * This is intended to clean-up if a daemon dies and can no longer
 * clean up it's child processes.
 */
void KillAppProcs(int host)
{
    if (RunParams.UseRMS) {

        /* Let RMS handle clean-up (should never get here) */

        return;

    } else if (RunParams.Local) {

        /* kill local processes */

        if (host == 0 && RunParams.AppPIDs) {
            ulm_err(("Killing processes\n"));
            for (int i = 0; i < RunParams.ProcessCount[host]; i++) {
                pid_t pid = RunParams.AppPIDs[0][i];
                if (RunParams.Verbose) {
                    ulm_err(("Executing \"kill(%d, SIGKILL)\"\n", pid));
                }
                if (kill(pid, SIGKILL) < 0) {
                    ulm_dbg(("kill: %d - No such process\n", pid));
                }
            }
        }

        if (0) {
            ulm_dbg(("Killing process group\n"));
            kill(0, SIGKILL);
        }

    } else if (RunParams.UseBproc) {

        /*
         * Be aggressive: Send kill to any process we can
         * (local PIDs are the same as remote PIDs)
         */

        if (RunParams.AppPIDs) {
            ulm_err(("Killing processes\n"));
            for (int h = 0; h < RunParams.NHosts; h++) {
                for (int p = 0; p < RunParams.ProcessCount[h]; p++) {
                    pid_t pid = RunParams.AppPIDs[h][p];
                    if (RunParams.Verbose) {
                        ulm_err(("Executing \"kill(%d, SIGKILL)\"\n", pid));
                    }
                    if (kill(pid, SIGKILL) < 0) {
                        ulm_dbg(("kill: %d - No such process\n", pid));
                    }
                }
            }
        }

        if (0) {
            ulm_dbg(("Killing process group\n"));
            kill(0, SIGKILL);
        }

    } else {

        /* try to kill using rsh */

        char cmd[ULM_MAX_COMMAND_STRING + 1];
        size_t n = 0;

        if (host < 0 || host >= RunParams.NHosts || !RunParams.AppPIDs) {
            return;
        }

        /* is host reachable? */
        putenv("PATH=/usr/bin:/usr/sbin:/bin:/sbin:/usr/etc");
        sprintf(cmd,
                "ping -c 1 %s > /dev/null  2>&1\n",
                RunParams.HostList[host]);
        if (system(cmd) != 0) {
            /* host can't be reached - return */
            return;
        }

        /* kill remote processes */
        n += sprintf(cmd + n, "rsh %s kill -9", RunParams.HostList[host]);
        for (int i = 0; i < RunParams.ProcessCount[host]; i++) {
            n += sprintf(cmd + n, " %u", RunParams.AppPIDs[host][i]);
            if (n >= ULM_MAX_COMMAND_STRING) {
                return;
            }
        }
        n += sprintf(cmd + n, " > /dev/null 2>&1\n");
        if (n >= ULM_MAX_COMMAND_STRING) {
            return;
        }
        system(cmd);
    }
}
