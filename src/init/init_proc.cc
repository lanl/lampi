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

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "init/init.h"
#include "client/ULMClient.h"
#include "util/dclock.h"

#include "ctnetwork/CTNetwork.h"
/*
 * This routine is used to set up initial process parameters
 */
void lampi_init_prefork_process_resources(lampiState_t *s)
{
    /*
     * Reset the max number of file descriptors which may be used by
     * the client daemon
     */

    struct rlimit rl;
    int returnValue;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_process_resources");
    }

    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rl);

    /*
     * Set-up unique core file names (if possible)
     */
    returnValue = setupCore();
    if (returnValue != ULM_SUCCESS) {
        s->error = ERROR_LAMPI_INIT_PREFORK_INITIALIZING_PROCESS_RESOURCES;
        return;
    }

    /* 
     * initialize lampi clock
     */
    init_dclock();

    /* 
     * initialize admin network
     */
#ifdef USE_CT	 
	CTNetworkInit();

    // set up pipes to redirect stderr/stdout so that even the daemon's
    // output is directed through the admin network.
    if ( s->useDaemon )
    {
        if ( (pipe(s->daemonSTDERR) < 0) || (pipe(s->daemonSTDOUT) < 0) )
        {
            ulm_ferr(("Host %s (pid = %d): Unable to create pipes for daemon.\n", _ulm_host(),
                      getpid()));
        }
        else
        {
            if ( (dup2(s->daemonSTDERR[1], STDERR_FILENO) < 0) ||
                 (dup2(s->daemonSTDOUT[1], STDOUT_FILENO) < 0) )
            {
                ulm_ferr(("Host %s (pid = %d): Unable to redirect stdio for daemon.\n", _ulm_host(),
                          getpid()));
            }
        }
    }
#endif

    /* 
     * Install signal handler 
     */
    returnValue = ClientInstallSigHandler();
    if (returnValue != ULM_SUCCESS) {
        s->error = ERROR_LAMPI_INIT_PREFORK_INITIALIZING_PROCESS_RESOURCES;
        return;
    }
}
