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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"

/*
 * Kill the application processes on a given host.  This is intended
 * to clean-up after client on that host terminates abnormally and can
 * no longer clean up it's child processes.
 */

void KillAppProcs(ULMRunParams_t *RunParameters, int host)
{
    if (RunParameters->UseRMS) {

      /* Let RMS handle clean-up (should never get here) */

        return;

    } else if (RunParameters->Local) {

      /* kill local processes */

        for (int i = 0; i < RunParameters->ProcessCount[host]; i++) {
            pid_t pid = RunParameters->AppPIDs[host][i];
            ulm_err(("Executing \"kill -9 %d\"\n", pid));
            if (kill(pid, 9) < 0) {
                ulm_dbg(("kill: %d - No such process\n", pid));
            }
        }

    } else if (RunParameters->UseBproc) {

        /*
	 * Be aggressive: Send kill to any process we can
	 * (local PIDs are the same as remote PIDs)
	 */

        ulm_err(("Killing processes\n"));
        for (int h = 0; h < RunParameters->NHosts; h++) {
            for (int p = 0; p < RunParameters->ProcessCount[h]; p++) {
                pid_t pid = RunParameters->AppPIDs[h][p];
		ulm_dbg(("Executing \"kill -9 %d\"\n", pid));
		if (kill(pid, 9) < 0) {
		    ulm_dbg(("kill: %d - No such process\n", pid));
		}
	    }
	}

	ulm_dbg(("Killing process group\n"));
        kill(0, 9);

    } else {

      /* try to kill using rsh */

        char cmd[ULM_MAX_COMMAND_STRING];

        /* is host reachable? */
        sprintf(cmd,
                "/usr/etc/ping -c 1 %s > /dev/null  2>&1\n",
                RunParameters->HostList[host]);
        if (system(cmd) != 0) {
	  /* host can't be reached - return */
            return;
        }

        /* kill remote processes */
        for (int i = 0; i < RunParameters->ProcessCount[host]; i++) {
            sprintf(cmd,
                    "rsh %s kill -9 %u  > /dev/null 2>&1\n",
                    RunParameters->HostList[host],
                    RunParameters->AppPIDs[host][i]);
            system(cmd);
        }
    }
}
