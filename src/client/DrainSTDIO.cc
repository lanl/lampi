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
#include <time.h>
#include <sys/time.h>           // needed for timespec
#include <sys/types.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"

/*
 * This routine drains data from the STDIO file descriptors, and shuts
 * down these descriptors.  The data is forwarded to mpirun for output
 * to user stderr and stdout of mpirun's session.
 */
void ClientDrainSTDIO(int *ClientStdoutFDs, int *ClientStderrFDs,
                      int ToServerStdoutFD, int ToServerStderrFD, int NFDs,
                      int MaxDescriptor, PrefixName_t *IOPreFix,
                      int *LenIOPreFix, size_t *StderrBytesWritten,
                      size_t *StdoutBytesWritten, int *NewLineLast,
                      int ControlSocketToULMRunFD, lampiState_t *state)
{
    bool again = true;
    int i;

    /* check to see if control socket is still open - if not exit */
#ifndef ENABLE_CT
    if (ControlSocketToULMRunFD == -1) {
        exit(5);
    }
#endif

    while (again) {
        ClientScanStdoutStderr(ClientStdoutFDs, ClientStderrFDs,
                               ToServerStdoutFD, ToServerStderrFD,
                               NFDs, MaxDescriptor, IOPreFix,
                               LenIOPreFix, StderrBytesWritten,
                               StdoutBytesWritten, NewLineLast, state);

        again = false;
        /* scan again only if pipes to children are still open; note
         * that we avoid the last set of descriptors since they are
         * the client daemon's....
         */
        for (i = 0; i < (NFDs - 1); i++) {
            if ((ClientStdoutFDs[i] >= 0) || (ClientStderrFDs[i] >= 0)) {
                again = true;
                break;
            }
        }
    }

    ClientStdoutFDs[NFDs - 1] = -1;
    ClientStderrFDs[NFDs - 1] = -1;
#ifndef ENABLE_CT
    dup2(ToServerStderrFD, STDERR_FILENO);
    dup2(ToServerStdoutFD, STDOUT_FILENO);
#endif
}
