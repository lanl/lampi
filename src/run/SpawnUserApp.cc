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
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

#include "init/environ.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/JobParams.h"
#include "run/Run.h"

/*
 * Spawn master process on remote host.  This routine also connects
 * standard out and standard error to one end of a pipe - one per host
 * - so that when a connection is established with the remote host,
 * stdout and stderr from the remote hosts can be read by ULMRun, and
 * processed.  ULMRun in turn will redirect this output to its own
 * stdout and stderr files - in this case the controlling tty.
 */

#define I_OWN(fs)   \
    ( (getuid() == fs.st_uid) && (S_IXUSR & fs.st_mode) )

#define GRP_OWN(fs) \
( (getgid() == fs.st_gid) && (S_IXGRP & fs.st_mode) )

int SpawnUserApp(unsigned int *AuthData, int ReceivingSocket, 
		int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		int FirstAppArgument, int argc, char **argv)
{
    int		isLocal, isValid;
	char    *execName = NULL;
    struct stat    fs;
    
    /*
     * Check that executable is valid.
     */
    execName = RunParameters->ExeList[0];
    if ( stat(execName, &fs) < 0 )
    {
        ulm_exit((-1, "Unable to determine status of executable %s."
                  "  Please verify that the executable exists.\n",
                 execName));
    }
    isValid = (fs.st_mode & S_IFREG) && ( I_OWN(fs) || GRP_OWN(fs)
                                         || (fs.st_mode & S_IXOTH) );
    if ( 0 == isValid )
    {
        ulm_err(("File %s is not executable by user.\n", execName));
        return -1;
    }
    
    /*
     * If LAMPI_LOCAL is set, then spawn all processes by execing
     * on the local host.  This is for debug purposes.
     */
    lampi_environ_find_integer("LAMPI_LOCAL", &isLocal);
    if ( (RunParameters->NHosts == 1) && (1 == isLocal) ) {
        return mpirun_spawn_exec(AuthData, ReceivingSocket,
                                 ListHostsStarted, RunParameters,
                                 FirstAppArgument, argc, argv);
    }
#ifdef ENABLE_RMS
    return mpirun_spawn_prun(AuthData, ReceivingSocket,
                             RunParameters, FirstAppArgument, argc, argv);
#endif

#ifdef ENABLE_BPROC
    return mpirun_spawn_bproc(AuthData, ReceivingSocket, ListHostsStarted,
                              RunParameters, FirstAppArgument, argc, argv);
#endif

    if (RunParameters->UseLSF) {
        return SpawnUserAppLSF(AuthData, ReceivingSocket, 
                               ListHostsStarted, RunParameters,
                               FirstAppArgument, argc, argv);
    }

    /*
     * Default: use rsh to spawn
     */
    return SpawnUserAppRSH(AuthData, ReceivingSocket, 
                           ListHostsStarted, RunParameters,
                           FirstAppArgument, argc, argv);
}
