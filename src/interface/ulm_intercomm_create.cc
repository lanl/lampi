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

#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "mpi/mpi.h"
#include "internal/log.h"
#include "queue/globals.h"
#include "internal/malloc.h"

/*!
 * create inter communicator
 */
extern "C" int ulm_intercomm_create(int localComm, int localLeader,
                                    int peerComm, int remoteLeader, int tag,
                                    int *newInterComm)
{
    int returnValue = MPI_SUCCESS;
    int *remoteGroupList = 0, errorCode,interCommContextID=-1;
    int iAmLocalLeader;
    bool inBothGroups;
    ULMRequest_t recvRequest, sendRequest;
    ULMStatus_t sendStatus,recvStatus;
    int localGlobalRank,remoteGlobalRank,generatingComm;

    if ((localComm > communicatorsArrayLen) || (localComm < 0)) {
        ulm_err(("Error: ulm_intercomm_create: bad communicator %d\n",
                 localComm));
        returnValue = MPI_ERR_COMM;
        goto BarrierTag;
    }

    if (communicators[localComm] == 0L) {
        ulm_err(("Error: ulm_intercomm_create: bad communicator %d\n",
                 localComm));
        returnValue = MPI_ERR_COMM;
        goto BarrierTag;
    }
    // verify that localLeader is valid
    if (localLeader >= communicators[localComm]->localGroup->groupSize) {
        returnValue = MPI_ERR_ARG;
	ulm_err(("Error: ulm_intercomm_create: bad localLeader.  localLeader %d groupSize %d\n",localLeader,communicators[localComm]->localGroup->groupSize));
        goto BarrierTag;
    }
    // verify that the peer communictor is valid
    if ((peerComm > communicatorsArrayLen) || (peerComm < 0)) {
        ulm_err(("Error: ulm_intercomm_create: bad communicator %d\n",
                 peerComm));
        returnValue = MPI_ERR_COMM;
        goto BarrierTag;
    }

    if (communicators[peerComm] == 0L) {
        ulm_err(("Error: ulm_intercomm_create: bad communicator %d\n",
                 peerComm));
        returnValue = MPI_ERR_COMM;
        goto BarrierTag;
    }
    // verify that remoteLeader is valid
    if (remoteLeader >=
        communicators[peerComm]->remoteGroup->groupSize) {
        returnValue = MPI_ERR_ARG;
	ulm_err(("Error: ulm_intercomm_create: Invalid remoteLeader - remoteLeader %d groupSize %d\n",remoteLeader,communicators[peerComm]->remoteGroup->groupSize));
        goto BarrierTag;
    }
    if (communicators[peerComm]->remoteGroup->ProcID == remoteLeader) {
	ulm_err(("Error: ulm_intercomm_create: Invalid remoteLeader II - remoteLeader %d ProcID %d\n",remoteLeader,communicators[peerComm]->remoteGroup->ProcID));
        returnValue = MPI_ERR_ARG;
        goto BarrierTag;
    }

    //
    // we will rely on the in-order delivery and reuse the "tag" for all
    //   peer-to-peer communications

    // increment localComm's reference count
    communicators[localComm]->refCounLock.lock();
    (communicators[localComm]->refCount)++;
    communicators[localComm]->refCounLock.unlock();
    communicators[localComm]->localGroup->incrementRefCount();
    communicators[localComm]->remoteGroup->incrementRefCount();

    //
    // exchange group lists
    //
    // send local group size and receive remote group size
    int remoteGroupSize;

    returnValue = peerExchange(peerComm, localComm, localLeader,
                               remoteLeader, tag,
                               &(communicators[localComm]->localGroup->
                                 groupSize), sizeof(int), &remoteGroupSize,
                               sizeof(int), &recvStatus);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }

    //
    // broadcast retmoreGroupSize to the rest of the local communicator
    //
    returnValue =
        ulm_bcast(&remoteGroupSize, sizeof(int),
                  (ULMType_t *)MPI_BYTE, localLeader, localComm);
    if (returnValue != ULM_SUCCESS) {
        returnValue = MPI_ERR_INTERN;
        goto BarrierTag;
    }
    // send/recv local group list (list of ranks in comm_world)
    remoteGroupList = (int *) ulm_malloc(sizeof(int) * remoteGroupSize);
    if (!remoteGroupList) {
        ulm_err(("Error: ulm_intercomm_create: Out of memory\n"));
        returnValue = MPI_ERR_NO_SPACE;
        goto BarrierTag;
    }
    returnValue = peerExchange(peerComm, localComm, localLeader,
                               remoteLeader, tag,
                               communicators[localComm]->localGroup->
                               mapGroupProcIDToGlobalProcID,
                               sizeof(int) *
                               communicators[localComm]->localGroup->
                               groupSize, remoteGroupList,
                               sizeof(int) * remoteGroupSize, &recvStatus);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    //
    // broadcast the new group information to the rest of the local communicator
    //
    returnValue = ulm_bcast(remoteGroupList, sizeof(int) * remoteGroupSize,
                            (ULMType_t *)MPI_BYTE, localLeader, localComm);
    if (returnValue != ULM_SUCCESS) {
        returnValue = MPI_ERR_INTERN;
        goto BarrierTag;
    }
    // verify that there is no overlap between the groups
    inBothGroups = false;
    for (int locProc = 0;
         locProc < communicators[localComm]->localGroup->groupSize;
         locProc++) {
        for (int remoteProc = 0; remoteProc < remoteGroupSize;
             remoteProc++) {
            if (communicators[localComm]->localGroup->
                mapGroupProcIDToGlobalProcID[locProc] == remoteGroupList[remoteProc]) {
                inBothGroups = true;
                break;
            }
        }                       // end remoteProc loop
        if (inBothGroups) {
            break;
        }
    }                           // end locProc loop

    if (inBothGroups) {
        ulm_err(("Error: ulm_intercomm_create: Communicator overlap\n"));
        returnValue = MPI_ERR_ARG;
        goto BarrierTag;
    }
    // create new remote group - relative to comm_world, since this is the only
    //   group that we know for sure will contain all the processes
    int remoteGroupIndex;
    int worldGroup;
    returnValue = ulm_comm_group(ULM_COMM_WORLD, &worldGroup);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    returnValue =
        ulm_group_incl(worldGroup, remoteGroupSize, remoteGroupList,
                       &remoteGroupIndex);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }

    /* 
     * generate new conextID
     */

    /* figure out if this process is one of the leader processes */
    iAmLocalLeader=0;
    if (communicators[localComm]->localGroup->ProcID == localLeader) {
	    iAmLocalLeader=1;
    }

    /* leader communicator , either local or remote, with lowest global rank root get new context ID 
     *   there is a collective call over the group to distribute this value, so all members of 
     *   the local communicator must call this, else the collectives calls no longer match up */
    localGlobalRank=communicators[localComm]->localGroup->ProcID;
    localGlobalRank=communicators[localComm]->localGroup->mapGroupProcIDToGlobalProcID[localGlobalRank];
    remoteGlobalRank=communicators[peerComm]->localGroup->mapGroupProcIDToGlobalProcID[remoteLeader];
    generatingComm=0;
    if ( localGlobalRank < remoteGlobalRank ) {
        errorCode=communicators[localComm]->getNewContextID(&interCommContextID);
        if (errorCode != ULM_SUCCESS) {
        	    returnValue = MPI_ERR_OTHER;
        	    goto BarrierTag;
        }
        generatingComm=1;
    }
    
    if( iAmLocalLeader ) {
    
	    /* new context ID is sent to the relevant remote-leader */
	    int tag = Communicator::INTERCOMM_TAG;
	    if ( localGlobalRank < remoteGlobalRank ) {
		    errorCode = ulm_isend(&interCommContextID, sizeof(int), 
				    (ULMType_t*)MPI_BYTE, remoteLeader, tag, peerComm, &sendRequest,
				    ULM_SEND_STANDARD);
		    if (errorCode != ULM_SUCCESS) {
			    ulm_err(("Error: returned in ulm_intercomm_create from ulm_isend :: %d\n",returnValue));
		    	    errorCode = MPI_ERR_OTHER;
		    	    goto BarrierTag;
		    }
		    errorCode = ulm_wait(&sendRequest, &sendStatus);
		    if (errorCode != ULM_SUCCESS) {
			    ulm_err(("Error: returned in ulm_intercomm_create from send ulm_wait :: %d\n",returnValue));
		    	    errorCode = MPI_ERR_OTHER;
		    	    goto BarrierTag;
		    }
	    } else {
		    errorCode = ulm_irecv(&interCommContextID, sizeof(int), 
				    (ULMType_t*)MPI_BYTE, remoteLeader, tag, peerComm, &recvRequest);
		    if (errorCode != ULM_SUCCESS) {
			    ulm_err(("Error: returned in ulm_intercomm_create from ulm_irecv :: %d\n",returnValue));
		    	    errorCode = MPI_ERR_OTHER;
		    	    goto BarrierTag;
		    }
		    errorCode = ulm_wait(&recvRequest, &recvStatus);
		    if (errorCode != ULM_SUCCESS) {
			    ulm_err(("Error: returned in ulm_intercomm_create from recv ulm_wait :: %d\n",returnValue));
		    	    errorCode = MPI_ERR_OTHER;
		    	    goto BarrierTag;
		    }
	    }
    }

    if( !generatingComm ) {
        /* broadcast the new context ID within the current communicator  - only communicator that
         *   did not already do so */
        returnValue = ulm_bcast(&interCommContextID, sizeof(int), (ULMType_t *)MPI_BYTE,
                      localLeader, localComm);
        if (returnValue != ULM_SUCCESS) {
            returnValue = MPI_ERR_INTERN;
    	ulm_err(("Error: returned in ulm_intercomm_create from recv ulm_bcast :: %d\n",returnValue));
            goto BarrierTag;
        }
    }

    // increment local group count
    // create new intercommunictor
    returnValue =
        ulm_communicator_alloc(interCommContextID,
                               communicators[localComm]->useThreads,
                               communicators[localComm]->localGroup->groupID,
                               remoteGroupIndex, false, 0, (int *) NULL,
                               Communicator::INTER_COMMUNICATOR, true, 1,
                               communicators[localComm]->useSharedMemForCollectives);
    if (returnValue != ULM_SUCCESS) {
        goto BarrierTag;
    }
    // set localGroupsComm
    communicators[interCommContextID]->localGroupsComm = localComm;

BarrierTag:
    int tmpreturnValue = returnValue;

    //
    // block until all processes are done - to make sure remote communication
    //   infrastructure is in place.
    //
    // barrier over local communictor
    returnValue = ulm_barrier(localComm);
    if (returnValue != ULM_SUCCESS) {
        if (remoteGroupList) {
            ulm_free(remoteGroupList);
        }
        return returnValue;
    }
    // exchange peer data
    returnValue = peerExchange(peerComm, localComm, localLeader,
                               remoteLeader, tag, (void *) NULL, 0,
                               (void *) NULL, 0, &recvStatus);
    if (returnValue != MPI_SUCCESS) {
        if (remoteGroupList) {
            ulm_free(remoteGroupList);
        }
        return returnValue;
    }
    // barrier over local communictor
    returnValue = ulm_barrier(localComm);
    if (returnValue != ULM_SUCCESS) {
        if (remoteGroupList) {
            ulm_free(remoteGroupList);
        }
        return returnValue;
    }

    if (remoteGroupList) {
        ulm_free(remoteGroupList);
    }

   /* set newInterComm */
    *newInterComm=interCommContextID;

    returnValue = tmpreturnValue;

    return returnValue;
}
