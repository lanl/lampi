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



/*
 * This routine is used to finish TotalView debugger setup - if need
 * be.  TotalView will be used to debug the application processes, as
 * well as the daemon processes.
 */
#include <stdio.h>
#include <stdlib.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/JobParams.h"
#include "run/TV.h"
#include "run/globals.h"

volatile int MPIR_acquired_pre_main = 1;

void MPIrunTVSetUpApp(pid_t ** PIDsOfAppProcs)
{
    int cnt, HostID, ProcID;

    /* return if no need for anything here - e.g. no debugging, or just
     *   debugging the startup, and the daemons */
    if (!((RunParameters.TVDebug) && (RunParameters.TVDebugApp)))
        return;

    /* fill in MPIR_proctable */
    cnt = 0;
    /* loop over hosts */
    for (HostID = 0; HostID < RunParameters.NHosts; HostID++) {
        /* loop over procs */
        for (ProcID = 0; ProcID < (RunParameters.ProcessCount)[HostID];
             ProcID++) {
            /* fill in application process information */
            MPIR_proctable[cnt].host_name =
                (char *) &((RunParameters.TVHostList)[HostID]);
            MPIR_proctable[cnt].executable_name =
                (char *) (RunParameters.ExeList[HostID]);
            MPIR_proctable[cnt].pid = PIDsOfAppProcs[HostID][ProcID];
#ifdef ENABLE_BPROC
            /* spawn xterm/gdb session to attach to remote process -- works in bproc env. only! */
            if (RunParameters.GDBDebug) {
                char title[64];
                char command[256];
                sprintf(title, "rank: %d (pid: %ld host: %s)", cnt, (long)MPIR_proctable[cnt].pid,
                        MPIR_proctable[cnt].host_name);
                sprintf(command, "xterm -title \"%s\" -e gdb %s %ld &", title, MPIR_proctable[cnt].executable_name,
                        (long)MPIR_proctable[cnt].pid);
                ulm_warn(("spawning: \"%s\"\n",command));
                system(command);
            }
#endif
            cnt++;
        }
    }

#ifdef ENABLE_BPROC
    /* if debugging with xterm/gdb in bproc env., then continue on... */
    if (RunParameters.GDBDebug) {
        return;
    }
#endif

    /*  set flag indicating TotalView is going to be used */
    MPIR_acquired_pre_main = 1;

    /* set debug state */
    MPIR_debug_state = MPIR_DEBUG_SPAWNED;

    /* provide function in which TotalView will set a breakpoint */
    MPIR_Breakpoint();
}
