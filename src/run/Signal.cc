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
#include <signal.h>

#include "internal/profiler.h"
#include "run/Run.h"
#include "run/RunParams.h"


static void handler(int signal)
{
    if (RunParams.Verbose) {
        ulm_err(("Caught signal %d\n", signal));
    }

    switch (signal) {

    case SIGALRM:
        /* return to correct location in mpirun */
        break;

    case SIGHUP:
    case SIGINT:
    case SIGTERM:
        Abort();
        break;
    }
}


int InstallSigHandler(void)
{
    struct sigaction sa;

    /*
     * Setup signal handling for specific signals.  This will include
     * specific signals, and which signals to block while the signal
     * handler is executing.
     */

    /*
     * SIGALRM - all signals will be blocked while execution is
     *            proceeding.
     */
    sa.sa_handler = handler;
    (void) sigfillset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL)) {
        ulm_err(("Error: trapping SIGHUP\n"));
        Abort();
    }
    if (sigaction(SIGINT, &sa, NULL)) {
        ulm_err(("Error: trapping SIGINT\n"));
        Abort();
    }
    if (sigaction(SIGTERM, &sa, NULL)) {
        ulm_err(("Error: trapping SIGTERM\n"));
        Abort();
    }

    return 0;
}
