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

#include "queue/Communicator.h"
#include "queue/contextID.h"
#include "internal/state.h"
#include "mpi/mpi.h"

//
// get new context id
//
int Communicator::getNewContextID(int *outputContextID)
{
    int returnValue = MPI_SUCCESS;
    int generateNewContextID = 0;
    int minRoot, remoteLeader, tag;
    ULMRequest_t recvRequest, sendRequest;
    ULMStatus_t sendStatus, recvStatus;


    /* 
     * determine if this process will get the new context ID 
     */

    if (communicatorType == Communicator::INTRA_COMMUNICATOR) {
        /* intra-communicator */
        if (localGroup->ProcID == 0)
            generateNewContextID = 1;
    } else {
        /* inter-communicator - we choose the process with the lowest
         *   global rank of the two commRoot procs to get the new
         *   context ID */
        minRoot = localGroup->mapGroupProcIDToGlobalProcID[0];
        if (remoteGroup->mapGroupProcIDToGlobalProcID[0] > minRoot)
            minRoot = remoteGroup->mapGroupProcIDToGlobalProcID[0];
        if (localGroup->ProcID == 0 &&
            minRoot == localGroup->mapGroupProcIDToGlobalProcID[0]) {
            generateNewContextID = 1;
        }
    }

    /* 
     * Obtain new context ID
     */

    /* generate new context ID */
    if (generateNewContextID) {
        if (usethreads()) {
            /* lock contextIDcontrol to guarantee atomic update */
            lampiState.contextIDCtl->idLock.lock();
        }

        /* get new context ID */
        *outputContextID = lampiState.contextIDCtl->nextID;

        /* reset nextID */
        lampiState.contextIDCtl->nextID++;
        /* if nextID is out of this process's range, reset
         *   to next stripe of data */
        if (lampiState.contextIDCtl->nextID == lampiState.contextIDCtl->outOfBounds) {
            lampiState.contextIDCtl->nextID += (lampiState.contextIDCtl->cycleSize -
                                                CTXIDBLOCKSIZE);
            lampiState.contextIDCtl->outOfBounds += lampiState.contextIDCtl->cycleSize;
        }
        if (usethreads()) {
            /* unlock contextIDcontrol */
            lampiState.contextIDCtl->idLock.unlock();
        }
    }

    /* send new context ID to rest of procs */
    if (communicatorType == Communicator::INTRA_COMMUNICATOR) {
        /* intra-communicator */
        returnValue = ulm_bcast(outputContextID, sizeof(int),
                                (ULMType_t *) MPI_BYTE, 0, contextID);
        if (returnValue != ULM_SUCCESS) {
            ulm_err(("Error: returned in Communicator::getNewContextID from ulm_bcast :: %d\n", returnValue));
        }
    } else {
        /* inter-communicator */

        /* root getting the context ID sends to the other root */
        if (localGroup->ProcID == 0) {
            if (generateNewContextID) {
                /* send the new context ID */
                tag = Communicator::INTERCOMM_TAG;
                /* root of remote group */
                remoteLeader = 0;
                returnValue = ulm_isend(outputContextID, sizeof(int),
                                        (ULMType_t *) MPI_BYTE,
                                        remoteLeader, tag, contextID,
                                        &sendRequest, ULM_SEND_STANDARD);
                if (returnValue != ULM_SUCCESS) {
                    ulm_err(("Error: returned in Communicator::getNewContextID from ulm_isend :: %d\n", returnValue));
                    return returnValue;
                }
                returnValue = ulm_wait(&sendRequest, &sendStatus);
                if (returnValue != ULM_SUCCESS) {
                    ulm_err(("Error: returned in Communicator::getNewContextID from send ulm_wait :: %d\n", returnValue));
                    return returnValue;
                }
            } else {
                /* receive the new context ID */
                tag = Communicator::INTERCOMM_TAG;
                /* root of remote group */
                remoteLeader = 0;
                returnValue = ulm_irecv(outputContextID, sizeof(int),
                                        (ULMType_t *) MPI_BYTE,
                                        remoteLeader, tag, contextID,
                                        &recvRequest);
                if (returnValue != ULM_SUCCESS) {
                    ulm_err(("Error: returned in Communicator::getNewContextID from ulm_irecv :: %d\n", returnValue));
                    return returnValue;
                }
                returnValue = ulm_wait(&recvRequest, &recvStatus);
                if (returnValue != ULM_SUCCESS) {
                    ulm_err(("Error: returned in Communicator::getNewContextID from recv ulm_wait :: %d\n", returnValue));
                    return returnValue;
                }
            }
        }

        /* broadcast the data to all other procs in local communicator */
        returnValue = ulm_bcast(outputContextID, sizeof(int),
                                (ULMType_t *) MPI_BYTE, 0,
                                localGroupsComm);
        if (returnValue != ULM_SUCCESS) {
            ulm_err(("Error: returned in Communicator::getNewContextID from 2nd ulm_bcast (%d)\n",
                     returnValue));
        }
    }

    return returnValue;
}
