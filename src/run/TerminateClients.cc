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
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/types.h"
#include "run/Run.h"
#include "client/SocketGeneric.h"
#include "internal/log.h"

/*
 *  This routine is used to send an abort message to all clients -
 *    before the fork - and then shutdown Client.
 */

void TerminateClients(int NClientsAccepted,
                      int *ListClientsAccepted,
                      ULMRunParams_t RunParameters)
{
    int hostIndex, j, listIndex;
    ulm_iovec_t SendIoVec[1];
    unsigned int Tags[1];
    int *SocketToClients =
        RunParameters.Networks.TCPAdminstrativeNetwork.SocketsToClients;

    /* loop over all hosts in job */
    for (hostIndex = 0; hostIndex < RunParameters.NHosts; hostIndex++) {

        /* check to see if connection was established with client  -
         *  loop over list of hosts that connected to mpirun */
        listIndex = -1;
        for (j = 0; j < NClientsAccepted; j++) {
            if (ListClientsAccepted[j] == hostIndex) {
                listIndex = hostIndex;
                break;
            }
        }

        if (listIndex > 0) {
            /* if connection established - send ABORTALL message to client */
            Tags[0] = ABORTALL;
            SendIoVec[0].iov_base = (char *) &Tags[0];
            SendIoVec[0].iov_len = (ssize_t) (sizeof(unsigned int));
            _ulm_Send_Socket(SocketToClients[listIndex], 1, SendIoVec);
        } else {
            /* if connection not established : write information to stderr */
            ulm_err(("Error: Can't connect to host : %s\n",
                     RunParameters.HostList[hostIndex]));
        }
        /* end loop over hosts */
    }

    /* terminate mpirun */
    exit(6);
}
