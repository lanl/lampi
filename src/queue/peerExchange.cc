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

#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"

/*
 * exchange data between peer communicator leaders
 */
int peerExchange(int peerComm, int localComm, int localLeader,
		 int remoteLeader, int tag, void *sendBuff,
                 ssize_t lenSendBuff, void *recvBuff, ssize_t lenRecvBuff,
                 ULMStatus_t *recvStatus )
{
    int returnValue = MPI_SUCCESS;
    ULMRequestHandle_t recvRequest, sendRequest;
    ULMStatus_t sendStatus;

    // only communicator leader participate
    if (communicators[localComm]->localGroup->ProcID == localLeader) {
        returnValue = ulm_isend(sendBuff, lenSendBuff, NULL, remoteLeader,
                                tag, peerComm, &sendRequest,
                                ULM_SEND_STANDARD);
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }

        returnValue = ulm_irecv(recvBuff, lenRecvBuff, NULL, remoteLeader,
                                tag, peerComm, &recvRequest);
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }

        returnValue = ulm_wait(&recvRequest, recvStatus);
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }

        returnValue = ulm_wait(&sendRequest, &sendStatus);
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }
    }

    return returnValue;
}
