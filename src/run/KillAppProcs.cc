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
 * This routine is used to kill the application processes on a given
 * host.  This is intended to clean-up after Client on that host
 * terminates abnormally and can no longer clean up it's child
 * processes.
 */

void mpirunKillAppProcs(HostName_t Host, int NProcs, pid_t *AppPIDs)
{
    int RetStat, i;
    char CommandString[ULM_MAX_COMMAND_STRING];

    /* 
     * check to see if host can be reached - if not, assume host is
     * unreachable.
     */
    sprintf(CommandString, "/usr/etc/ping -c 1 %s > /dev/null  2>&1\n",
            Host);
    RetStat = system(CommandString);

    /* if host can't be reached - return */
    if (RetStat != 0)
        return;

    /* loop over list of pid's */
    for (i = 0; i < NProcs; i++) {

        /* setup kill command string */
        sprintf(CommandString, "rsh %s kill -9 %u  > /dev/null 2>&1\n",
                Host, AppPIDs[i]);

        /* send kill command */
        system(CommandString);
    }
}
