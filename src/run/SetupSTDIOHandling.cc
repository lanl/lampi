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
#include <unistd.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "client/SocketServer.h"
#include "client/SocketGeneric.h"

/*
 * This subroutine is used to set up the handling special handling of
 *  standard error/out for the application process spawned by mpirun.
 *  The client side of the job also has some setup work to do.
 */

int MPIrunSetupSTDIOHandling(int **STDERRfds, int **STDOUTfds, int NHosts,
                             int *ControlFDs, HostName_t *ClientList,
                             ssize_t ** StderrBytesRead,
                             ssize_t ** StdoutBytesRead)
{
    int i, RetVal;
    int AcceptErrFD, AcceptOutFD;
    int StdoutPORTID, StderrPORTID;
    int TmpBuff[2];
    unsigned int Tag;
    ssize_t IOReturn;
    ulm_iovec_t IOVec[2];

    /* setup STDOUT listening socket */
    StdoutPORTID = SetupAcceptingSocket(&AcceptOutFD);
    if (StdoutPORTID < 0)
        return StdoutPORTID;

    /* setup STDERR listening socket */
    StderrPORTID = SetupAcceptingSocket(&AcceptErrFD);
    if (StderrPORTID < 0)
        return StderrPORTID;

    /* send port ID's to Clients so that they can connect to appropriate FD */
    Tag = STDIOFDS;
    IOVec[0].iov_base = &Tag;
    IOVec[0].iov_len = (ssize_t) (sizeof(unsigned int));
    IOVec[1].iov_base = TmpBuff;
    IOVec[1].iov_len = (ssize_t) (2 * sizeof(int));
    TmpBuff[0] = StdoutPORTID;
    TmpBuff[1] = StderrPORTID;
    for (i = 0; i < NHosts; i++) {
        IOReturn = _ulm_Send_Socket(ControlFDs[i], 2, IOVec);
        if (IOReturn < 0) {
            printf("Error: sending STDIOFDS.  RetVal: %ld\n", IOReturn);
            Abort();
        }
    }

    /* accept connections for STDOUT data */
    RetVal =
        AcceptSocketConnections(AcceptOutFD, STDOUTfds, ClientList,
                                NHosts);
    if (RetVal < 0)
        return RetVal;


    /* accept connections for STDERR data */
    RetVal =
        AcceptSocketConnections(AcceptErrFD, STDERRfds, ClientList,
                                NHosts);
    if (RetVal < 0)
        return RetVal;

    /* initialize stderr/stdout byte count */
    (*StderrBytesRead) = ulm_new(ssize_t,  NHosts);
    for (i = 0; i < NHosts; i++)
        (*StderrBytesRead)[i] = 0;

    (*StdoutBytesRead) = ulm_new(ssize_t,  NHosts);
    for (i = 0; i < NHosts; i++)
        (*StdoutBytesRead)[i] = 0;

    /* release extra resources */
    close(AcceptOutFD);
    close(AcceptErrFD);

    return 0;
}
