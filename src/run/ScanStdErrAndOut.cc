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



#include "internal/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>             // needed for bzero()

#include "internal/constants.h"
#include "internal/types.h"
#include "run/Run.h"
#include "client/SocketGeneric.h"

/*
 *  This routine is used to read the stderr/stdout data
 *   from the user applications.
 */

int mpirunScanStdErrAndOut(int *STDERRfds, int *STDOUTfds, int NHosts,
                           int MaxDescriptor, ssize_t *StderrBytesRead,
                           ssize_t *StdoutBytesRead)
{
    int i, RetVal;
    ssize_t DataBytes;
    fd_set ReadSet;
    struct timeval WaitTime;

    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 10000;

    /* check to see if there is any data to read */
    FD_ZERO(&ReadSet);
    for (i = 0; i < NHosts; i++)
        if (STDERRfds[i] > 0)
            FD_SET(STDERRfds[i], &ReadSet);
    for (i = 0; i < NHosts; i++)
        if (STDOUTfds[i] > 0)
            FD_SET(STDOUTfds[i], &ReadSet);

    RetVal = select(MaxDescriptor, &ReadSet, NULL, NULL, &WaitTime);
    if (RetVal < 0)
        return RetVal;

    /* read data and write to mpirun stdout/stderr */
    if (RetVal > 0) {
        for (i = 0; i < NHosts; i++) {
            if ((STDERRfds[i] > 0) && (FD_ISSET(STDERRfds[i], &ReadSet))) {
                /* clear this descriptor from ReadSet in case it is also STDOUTfds[i] */
                FD_CLR(STDERRfds[i], &ReadSet);
                /* now read this descriptor... */
                DataBytes = mpirunRecvAndWriteData(STDERRfds[i], stderr);
                StderrBytesRead += DataBytes;
                if (DataBytes == 0) {
                    close(STDERRfds[i]);
                    STDERRfds[i] = -1;
                }
            }
        }                       /* end loop over stderr descriptors */

        for (i = 0; i < NHosts; i++) {
            if ((STDOUTfds[i] > 0) && (FD_ISSET(STDOUTfds[i], &ReadSet))) {
                DataBytes = mpirunRecvAndWriteData(STDOUTfds[i], stdout);
                if (DataBytes == 0) {
                    close(STDOUTfds[i]);
                    STDOUTfds[i] = -1;
                }
                if (DataBytes > 0)
                    StdoutBytesRead += DataBytes;
            }
        }                       /* end loop over stdout descriptors */
    }

    return 0;
}
