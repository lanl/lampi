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

#include "queue/globals.h"
#include "queue/globals.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "ulm/ulm.h"

#ifdef USE_ELAN_COLL
#include "path/quadrics/path.h"
#include "path/quadrics/state.h"
#endif

#ifdef USE_ELAN_COLL
extern "C" int ulm_alloc_bcaster(int new_comm, int useThreads)
{
  int                 errorcode = ULM_SUCCESS;
  int                 bcaster_id;
  char                busy_bcasters_in[MAX_BROADCASTERS];
  char                busy_bcasters_out[MAX_BROADCASTERS];
  int                 tmp, gotit, vote, all_vote;

  START_MARK;

  Communicator       *cp ; 
  cp      = communicators[new_comm];

  int GlobalProcID_min = cp->localGroup->mapGroupProcIDToGlobalProcID[0];

  if (communicators[new_comm]->localGroup->numberOfHostsInGroup <=1)
    return errorcode;

  /* make sure we all agree we are using hardware broadcast: */
  /* one cpu may disable after detecting an error  */
  ulm_allreduce(&quadricsHW, &tmp, 1, (ULMType_t *) MPI_INT, 
	  (ULMOp_t *) MPI_BAND, new_comm );
	  /*(ULMOp_t *) MPI_LAND, new_comm );*/
  quadricsHW = tmp;

  if (!quadricsHW) // quadrics hardware bcast disabled by user
      return errorcode;

  /* leave ULM_COMM_WORLD and ULM_COMM_SELF after postfork_path */
  if ( new_comm == ULM_COMM_SELF || new_comm == ULM_COMM_WORLD )
    goto barrier_and_exit;

  /* It is also important to avoid using locks within a collective
     operation, which may cause deadlock across processes over different
     nodes */

  /* make sure only the processes in the new communicator 
   * are allocated with the broadcaster */
  if ( new_comm == MPI_COMM_NULL ) goto barrier_and_exit;

  /* try to find a free broadcaster */
  /* busy_broadcasters[i].cid = MPI_COMM_NULL    bcaster_id NOT in use */
  /* busy_broadcasters[i].cid = cp->contextID    bcaster_id used by cp */

  for (int i=0; i<broadcasters_array_len; ++i) 
      busy_bcasters_in[i]=(busy_broadcasters[i].cid!=MPI_COMM_NULL);

  ulm_allreduce(busy_bcasters_in, busy_bcasters_out,
	  broadcasters_array_len, (ULMType_t *) MPI_CHAR, 
	  (ULMOp_t *) MPI_BOR, new_comm );

  /* To get the first free broacasters */
  for (bcaster_id = 0; bcaster_id < broadcasters_array_len; bcaster_id ++)
  {
    if ( busy_bcasters_out[bcaster_id] == 0) break;
  }

  /* Make sure the available communicator is still within range,
   * otherwise, output a prompt message and quit */
  if ( bcaster_id >= broadcasters_array_len )  {
      if (cp->localGroup->ProcID==0) {
          ulm_err(("Warning: exhausted hw bcasters. Reverting to software bcast for communicator=%i\n",new_comm));
      }
      goto barrier_and_exit;
  }

  /* we have all agreed to use bcaster_id.  
     See if it is still available on this node */
  broadcasters_locks->lock();
  if (MPI_COMM_NULL == busy_broadcasters[bcaster_id].cid) {
      busy_broadcasters[bcaster_id].cid = new_comm;
      busy_broadcasters[bcaster_id].pid = GlobalProcID_min;
      gotit = 1;
      vote  = 1;
  } else {
       /* I didn't get the lock, but if any other proc on this node 
	* and also in my communicator got it, that is good enough.  
        *
        * Note:  "new_comm" may not be unique accross disjoint communicators,
        * so we need to match both "new_comm" and smallest proc id in new_comm.
        */
	gotit = 0;
	vote  = 0;
	if (busy_broadcasters[bcaster_id].cid == new_comm && 
		busy_broadcasters[bcaster_id].pid == GlobalProcID_min) 
	    vote = 1;
  }
  broadcasters_locks->unlock();

  ulm_allreduce(&vote, &all_vote, 1,
      (ULMType_t *) MPI_INT, (ULMOp_t *) MPI_LAND, new_comm );


  if (0 == all_vote) {  /* we failed to agree */
      if (1 == gotit) { /* give up the broadcaster */
          broadcasters_locks->lock();
          busy_broadcasters[bcaster_id].cid = MPI_COMM_NULL;
          broadcasters_locks->unlock();  
      }

      /* TODO:  create a centralized arbritrator and try again... */
      if (cp->localGroup->ProcID == 0) {
          ulm_err(("Warning: hw bcast shared memory arbritration failed."
          " Reverting to software bcast. comm=%i \n", new_comm));
      }
      goto barrier_and_exit;
  }

  {
    Broadcaster        *bcaster;

    /* Assign the first available broadcaster to the communicator*/
    bcaster = quadrics_broadcasters[bcaster_id];
    assert(bcaster_id == bcaster->id && bcaster_id != 0 
        && bcaster->inuse == 0);

    /* Link back to the communicator */
    bcaster->comm_index = cp->contextID;
    bcaster->inuse == 1;
    
    /* Enable the hardware multicast */
    errorcode = bcaster->hardware_coll_init();

    if ( errorcode == ULM_SUCCESS)
    {
#if 0
      if (cp->localGroup->ProcID == 0)  
	ulm_err(("%i Enabling hw bcast (%i) on comm=%i\n",
	      myproc(), bcaster_id, new_comm ));
#endif

      cp->hw_bcast_enabled = QSNET_COLL ;
      cp->bcaster          = bcaster;
      bcaster->inuse == 1;
    }
    else
    {
      /* Do a free for safety reasons */
      bcaster->inuse == 0;
      bcaster->broadcaster_free();
      cp->bcast_queue.handle = 0;
      cp->hw_bcast_enabled = 0;
      cp->bcaster          = 0;
    }
  }

barrier_and_exit:
  /* Synchronize all processes within the old communicator */
  ulm_barrier(new_comm);


  END_MARK;
  return errorcode;
}
#endif

/*
 * This routine is used to set up a group of prcesses identified
 * by the id comm.
 */
extern "C" int ulm_communicator_alloc(int comm, int useThreads, int group1Index,
                                      int group2Index, int setCtxCache, int sizeCtxCache, int *lstCtxElements,
                                      int commType, int okToDeleteComm, int useSharedMem, int useSharedMemForColl)
{
    int returnCode = ULM_SUCCESS;
    Communicator **tmpPtr;

    //
    // Check to make sure group size is within the size of the
    // global group
    //

    if (ENABLE_CHECK_API_ARGS) {
        if (group1Index >= grpPool.poolSize || group1Index < 0) {
            ulm_err(("Error: group index out of range\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (group2Index >= grpPool.poolSize || group2Index < 0) {
            ulm_err(("Error: group index out of range\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (grpPool.groups[group1Index] == 0) {
            ulm_err(("Error: null group\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (grpPool.groups[group2Index] == 0) {
            ulm_err(("null group\n"));
            return ULM_ERR_BAD_PARAM;
        }
    }
    //
    // find communicator marked for deletion, and see if the can
    //   be deleted. - lazy free
    //
    if (usethreads())
        communicatorsLock.lock();
    /* we really only want one thread at a time trying to free communicators */
    for (int cm = 0; cm < communicatorsArrayLen; cm++) {
        // check to see if marked for deletion
        if (communicators[cm]
            && (communicators[cm]->markedForDeletion == 1)) {

            returnCode = ulm_communicator_really_free(cm);
            if (returnCode != ULM_SUCCESS) {
                if (usethreads())
                    communicatorsLock.unlock();
                return returnCode;
            }
        }
    }                           // end cm loop

    /* lock to make sure only one thread at a time will modify the communicators
     *  and activeCommunicators array
     */

    // grow communictor array if array is not large enough to hold
    //   pointer to new group
    //
    if (communicatorsArrayLen <= comm) {
        int oldLen = communicatorsArrayLen;
        communicatorsArrayLen = comm + nIncreaseCommunicatorsArray + 1;

        // grow array
        /* if threaded, lock to make sure only one thread at a time changes
         *   this array.  Also, make sure that if the pointer is changed,
         *   and this is a threaded job, the job aborts - want to make sure
         *   that if other threads reference communictor, they do not end
         *   up holding on to stale pointer, and don't want to take the
         *   hit of making the array a volatile array */
        /* check to see if this has already been reallocated */
        tmpPtr = communicators;
        communicators = (Communicator **) realloc((void *) communicators,
                                                  communicatorsArrayLen *
                                                  sizeof(Communicator *));
        if (!communicators) {
            ulm_err(("Error: Out of memory\n"));
            if (usethreads())
                communicatorsLock.unlock();
            return ULM_ERR_OUT_OF_RESOURCE;
        }

        if ((tmpPtr != communicators) && usethreads()) {
            ulm_err(("Error: ulm_communicator_alloc: "
                     " The communicator array moved with the realloc call - stale pointers possible\n"));
            if (usethreads())
                communicatorsLock.unlock();
            return ULM_ERROR;
        }
        // initialize new  elements
        for (int grp = oldLen; grp < communicatorsArrayLen; grp++)
            communicators[grp] = 0L;

        if (usethreads())
            communicatorsLock.unlock();
    }
    // make sure that this context id is not locally in use
    if ((comm > communicatorsArrayLen)
        && (communicators[comm] != 0L)) {
        ulm_err(("Error: communicator already in use (%d)\n",
                 comm));
        if (usethreads())
            communicatorsLock.unlock();
        return ULM_ERR_COMM;
    }

    /* get element from Queue pool */
    bool firstInstanceOfContext = true;

    //
    // set communicators array
    //
    // allocate communicator object
    communicators[comm] = ulm_new(Communicator, 1);
    if (!communicators[comm]) {
        ulm_err(("Error: Out of memory\n"));
        if (usethreads())
            communicatorsLock.unlock();
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // increment the reference counter
    communicators[comm]->refCount = 1;

    /* need to check and make sure that activeCommunicators can hold
     *   another entry */


    activeCommunicators[nCommunicatorInstancesInUse] = comm;
    nCommunicatorInstancesInUse++;

    //
    // call group init function to set up the group object
    //
    bool threadUsage = (bool) usethreads();

    // initialize the Communicator
    returnCode =
        communicators[comm]->init(comm, threadUsage, group1Index,
                                  group2Index, firstInstanceOfContext,
                                  setCtxCache, sizeCtxCache,
                                  lstCtxElements, commType, okToDeleteComm,
                                  useSharedMem, useSharedMemForColl);

    if (usethreads())
        communicatorsLock.unlock();

    return returnCode;
}
