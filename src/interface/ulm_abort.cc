/*
 * Copyright 2002-2003. The Regents of the University of
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
#include <signal.h>
#include <unistd.h>

#include "internal/log.h"
#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*!
 * User abort function
 *
 * \param comm		Communicator ID
 * \param errorCode	Exit code to pass to exit()
 * \return		Function never returns
 */
extern "C" int _ulm_abort(int comm, int error, char *file, int line)
{
    struct sigaction action, oldaction;

    _ulm_set_file_line(file, line);
    _ulm_log("Client aborting (comm=%d, error=%d)\n", comm, error);

    if (lampiState.AbnormalExit) {
        /* block SIGCHLD processing */
        action.sa_handler = SIG_IGN;
        action.sa_flags = 0;
        sigaction(SIGCHLD, &action, &oldaction);
        ATOMIC_LOCK(lampiState.AbnormalExit->lock);
        if (lampiState.AbnormalExit->flag == 0) {
            /* store our information for the client daemon */
            lampiState.AbnormalExit->flag = 1;
            lampiState.AbnormalExit->pid = getpid();
            lampiState.AbnormalExit->signal = 0;
            lampiState.AbnormalExit->status = error;
        }
        ATOMIC_UNLOCK(lampiState.AbnormalExit->lock);
        /* unblock SIGCHLD processing */
        sigaction(SIGCHLD, &oldaction, NULL);
    }

    fflush(stdout);
    fflush(stderr);
#if ENABLE_RMS
    raise(SIGHUP);
#else
    exit(error);
#endif

    // never reached but shuts up nosy compilers...
    return error;
}
