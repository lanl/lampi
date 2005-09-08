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
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

#include "internal/profiler.h"
#include "init/environ.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/RunParams.h"
#include "run/Run.h"

/*
 * Spawn master process on remote host.  This routine also connects
 * standard out and standard error to one end of a pipe - one per host
 * - so that when a connection is established with the remote host,
 * stdout and stderr from the remote hosts can be read by ULMRun, and
 * processed.  ULMRun in turn will redirect this output to its own
 * stdout and stderr files - in this case the controlling tty.
 */

int Spawn(unsigned int *AuthData, int ReceivingSocket,
          int **ListHostsStarted,
          int argc, char **argv)
{
    int isValid;
    char *execName = NULL;
    struct stat fs;

    /*
     * Fix for VAPI shared libraries
     */
    if (RunParams.Networks.UseIB) {
        putenv("NO_IB_PREMAIN_INIT=1");
    }

    /*
     * Check that executable is valid, but only exit on systems where
     * where the executable is guaranteed to be on the front end.
     */
    if (access(RunParams.ExeList[0], X_OK) < 0) {
        if (RunParams.UseRMS || RunParams.UseBproc || RunParams.Local) {
            ulm_err(("Error: Can't execute \"%s\": %s\n",
                     RunParams.ExeList[0], strerror(errno)));
            exit(EXIT_FAILURE);
        } else {
            ulm_warn(("Warning: Can't execute \"%s\" on the local host: %s\n",
                      RunParams.ExeList[0], strerror(errno)));
        }
    }

    /*
     * Should we run locally?  Yes if -local option given, or
     * LAMPI_LOCAL is set.
     */
    if (RunParams.Local == 0) {
        lampi_environ_find_integer("LAMPI_LOCAL", &(RunParams.Local));
    }
    if ((RunParams.NHosts == 1) && RunParams.Local) {
        if (RunParams.Verbose) {
            ulm_err(("*** Spawning application using fork/exec\n"));
        }
        return SpawnExec(AuthData, ReceivingSocket, ListHostsStarted, argc, argv);
    }

    /*
     * Override with ssh start-up if the user wants it...
     */
    if (RunParams.UseSSH) {
        if (RunParams.Verbose) {
            ulm_err(("*** Spawning application using ssh\n"));
        }
        return SpawnRsh(AuthData, ReceivingSocket, ListHostsStarted, argc, argv);
    }

    if (ENABLE_RMS) {
        if (RunParams.Verbose) {
            ulm_err(("*** Spawning application using RMS\n"));
        }
        return SpawnRms(AuthData, ReceivingSocket, argc, argv);
    }

    if (ENABLE_BPROC) {
        if (RunParams.Verbose) {
            ulm_err(("*** Spawning application using BProc\n"));
        }
        return SpawnBproc(AuthData, ReceivingSocket, ListHostsStarted, argc, argv);
    }

    if (RunParams.UseLSF) {
        if (RunParams.Verbose) {
            ulm_err(("*** Spawning application using LSF\n"));
        }
        return SpawnLsf(AuthData, ReceivingSocket,  ListHostsStarted, argc, argv);
    }

    /*
     * Default: use rsh to spawn
     */
    if (RunParams.Verbose) {
        ulm_err(("*** Spawning application using default method (rsh/ssh)\n"));
    }
    return SpawnRsh(AuthData, ReceivingSocket, ListHostsStarted, argc, argv);
}
