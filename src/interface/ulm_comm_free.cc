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

#include "queue/globals.h"
#include "internal/log.h"
#include "internal/mpi.h"
#include "internal/new.h"
#include "mpi/constants.h"
#include "ulm/ulm.h"


int ulm_communicator_really_free(int comm)
{
    int returnCode = MPI_SUCCESS;
    int commInUse;
    int local_grp_id, remote_grp_id;

    /*
     * lock to make sure only one thread at a time will modify the
     * communicators and activeCommunicators array
     */

    // make sure comm is in use
    if (communicators[comm] == 0L) {
        ulm_err(("Error: ulm_communicator_really_free: communicator %d not in use\n", comm));
        return MPI_ERR_COMM;
    }
    // check to see if queues are empty
    bool okToDelete = communicators[comm]->areQueuesEmpty();

    // if pending traffic - return
    if (!okToDelete) {
        return MPI_SUCCESS;
    }
    // reset activeCommunicators
    for (commInUse = 0; commInUse < nCommunicatorInstancesInUse;
         commInUse++) {
        // find the group in use
        if (activeCommunicators[commInUse] == comm) {
            // this is group being freed
            for (int restGrps = commInUse;
                 restGrps < nCommunicatorInstancesInUse - 1; restGrps++) {
                // reset activeCommunicators array
                activeCommunicators[restGrps] =
                    activeCommunicators[restGrps + 1];
            }
            /* poison the last soon-to-be-unused entry */
            activeCommunicators[nCommunicatorInstancesInUse - 1] = -1;
            break;
        }
    }

    // if it's not in activegroups, then already freed...
    if (commInUse == nCommunicatorInstancesInUse) {
        ulm_err(("Error: ulm_communicator_really_free: "
                 "%d comm not in activeCommunicators. Group not in use\n.",
                 comm));
        return MPI_ERR_COMM;
    }
    // reset nCommunicatorInstancesInUse
    // this must be decremented now to keep in sync with activeCommunicators
    // no matter what else happens later...
    nCommunicatorInstancesInUse--;

    // save off group ids
    local_grp_id = communicators[comm]->localGroup->groupID;
    remote_grp_id = communicators[comm]->remoteGroup->groupID;

    // call member function to free resources
    returnCode = communicators[comm]->freeCommunicator();

    // free the actaul element
    ulm_delete(communicators[comm]);

    // remove communicator from local list
    communicators[comm] = 0L;

    // free associated groups
    returnCode = ulm_group_free(local_grp_id);
    if (returnCode != MPI_SUCCESS) {
        ulm_err((" error freeing local group.  returnCode %d\n",
                 returnCode));
        return MPI_ERR_OTHER;
    }
    returnCode = ulm_group_free(remote_grp_id);
    if (returnCode != MPI_SUCCESS) {
        ulm_err((" error freeing remote group.  returnCode %d\n",
                 returnCode));
        return MPI_ERR_OTHER;
    }

    return returnCode;
}


extern "C" int ulm_comm_free(int comm)
{
    int returnCode = MPI_SUCCESS;
    int cm;

    // make progress
    returnCode = ulm_make_progress();
    if (returnCode != ULM_SUCCESS) {
        return MPI_ERR_INTERN;
    }
    // make sure that this communicator can be deleted
    if (!(communicators[comm]->canFreeCommunicator)) {
        return MPI_ERR_COMM;
    }
    //
    // is reference count <= 0 ?
    if (communicators[comm]->refCount <= 0) {
        return MPI_ERR_COMM;
    }
    //
    // check to see if the standard allows this communicator to be deleted
    //   Whether of not the queues are empty will be checked later.
    //

    //
    // decrement refCount for localGroupsComm - if INTER_COMMUNICATOR
    //
    if (communicators[comm]->communicatorType ==
        Communicator::INTER_COMMUNICATOR) {
        int ret = ulm_comm_free(communicators[comm]->localGroupsComm);
        if (ret != MPI_SUCCESS) {
            return ret;
        }
    }
    //
    // run delete functions code attributes
    //
    int canDeleteComm = 1;
    int ret;
    int returnValue = MPI_SUCCESS;
    int keyvalInUse;
    for (int attrib = 0;
         attrib < communicators[comm]->sizeOfAttributeArray; attrib++) {
        int keyval = communicators[comm]->attributeList[attrib].keyval;
        if (keyval == -1)
            continue;
        Communicator *commPtr = communicators[comm];
        /* check to see if keyval is already in use */
        keyvalInUse = -1;
        for (int key = 0; key < commPtr->sizeOfAttributeArray; key++) {
            if (commPtr->attributeList[key].keyval == keyval) {
                keyvalInUse = key;
                break;
            }
        }

        /* run the delete functions, and check to see if we can delete this
         *   communicator
         */
        if (attribPool.attributes[keyval].setFromFortran) {
            /* fortran version */
            attribPool.attributes[keyval].fDeleteFunction(&comm, &keyval,
                                                          (int *)
                                                          &(commPtr->
                                                            attributeList
                                                            [keyvalInUse].
                                                            attributeValue),
                                                          (int
                                                           *) (attribPool.
                                                               attributes
                                                               [keyval].
                                                               extraState),
                                                          &ret);
        } else {
            /* c version */
            ret =
                attribPool.attributes[keyval].cDeleteFunction(comm, keyval,
                                                              commPtr->
                                                              attributeList
                                                              [keyvalInUse].
                                                              attributeValue,
                                                              attribPool.
                                                              attributes
                                                              [keyval].
                                                              extraState);
        }
        if (ret != MPI_SUCCESS) {
            canDeleteComm = 0;
            returnValue = ret;
            break;
        }
    }
    /* check to see if any of the delete functions prevent us from deleting
     *  this communicator
     */
    if (!canDeleteComm)
        return returnValue;

    /* decrement attribute refernce count */
    for (int attrib = 0;
         attrib < communicators[comm]->sizeOfAttributeArray; attrib++) {
        int keyval = communicators[comm]->attributeList[attrib].keyval;
        if (keyval == -1)
            continue;
        Communicator *commPtr = communicators[comm];
        /* check to see if keyval is already in use */
        for (int key = 0; key < commPtr->sizeOfAttributeArray; key++) {
            if (commPtr->attributeList[key].keyval == keyval) {
                keyvalInUse = key;
                break;
            }
        }

        attribPool.attributes[keyval].Lock.lock();
        attribPool.attributes[keyval].refCount--;
        attribPool.attributes[keyval].Lock.unlock();
    }

    /* ok to decrement refernece count */
    communicators[comm]->refCounLock.lock();
    (communicators[comm]->refCount)--;
    if (communicators[comm]->refCount == 0)
        communicators[comm]->markedForDeletion = 1;
    communicators[comm]->refCounLock.unlock();

    // find communicator marked for deletion, and see if the can
    //   be deleted.
    if (usethreads())
        communicatorsLock.lock();
    for (cm = 0; cm < communicatorsArrayLen; cm++) {
        // check to see if marked for deletion
        if (communicators[cm]
            && (communicators[cm]->markedForDeletion == 1)) {
            returnCode = ulm_communicator_really_free(cm);
            if (returnCode != MPI_SUCCESS) {
                if (usethreads())
                    communicatorsLock.unlock();
                return returnCode;
            }
        }
    }                           // end cm loop
    if (usethreads())
        communicatorsLock.unlock();

    return returnCode;
}
