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
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"

/*
 * This routine sets the signal handling for Client
 */
int ClientInstallSigHandler(void)
{
    struct sigaction sa;

    /*
     * Setup signal handling for specific signals.  This will include
     * specific signals, and which signals to block while the signal
     * handler is executing.
     */

    /*
     * SIGCHLD - all signals will be blocked while execution is
     *            proceeding.  Used to make sure that under abnormal
     *            termination conditions, all other worker process on
     *            the given host are terminated, and then the process
     *            of terminating the rest of the process is started.
     */
    sa.sa_handler = Clientsig_child;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGCHLD\n"));
    }
    if (sigaction(SIGILL, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGILL\n"));
    }
    if (sigaction(SIGABRT, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGABRT\n"));
    }
    if (sigaction(SIGFPE, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGFPE\n"));
    }

    /*
       if( sigaction(SIGSEGV, &sa,NULL)) {
       ulm_exit((-1, "Error: trapping SIGSEGV\n"));
       }
     */

    if (sigaction(SIGPIPE, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGPIPE\n"));
    }

    /*
       if( sigaction(SIGBUS, &sa,NULL)) {
       ulm_exit((-1, "Error: trapping SIGBUS\n"));
       }
     */

    if (sigaction(SIGHUP, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGHUP\n"));
    }
    if (sigaction(SIGINT, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGINT\n"));
    }
    if (sigaction(SIGQUIT, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGQUIT\n"));
    }
    if (sigaction(SIGTRAP, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGTRAP\n"));
    }
    if (sigaction(SIGSYS, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGSYS\n"));
    }
    if (sigaction(SIGALRM, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGALRM\n"));
    }
    if (sigaction(SIGTERM, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGCHLD\n"));
    }
    if (sigaction(SIGURG, &sa, NULL)) {
        ulm_exit((-1, "Error: trapping SIGURG\n"));
    }

    return 0;
}
