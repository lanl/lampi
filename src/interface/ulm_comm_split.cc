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
#include "internal/options.h"
#include "ulm/ulm.h"
#include "mpi/mpi.h"
#include "internal/log.h"
#include "queue/globals.h"
#include "internal/malloc.h"

/*!
 * Get the communicator's rank
 */
extern "C" int ulm_comm_split(int comm, int color, int key, int *newComm)
{
    // temp arrays
    int *allPairs=0;
    int *list=0;
    bool *listDone=0;
    int *sortedList=0;
    int returnValue;
    int groupSize,nMyColor, cnt;

    if (OPT_CHECK_API_ARGS) {

	if( (comm > communicatorsArrayLen) || ( comm < 0 ) ) {
	    ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
		     comm));
	    returnValue=MPI_ERR_COMM;
	    goto BarrierTag;
	}

	if (communicators[comm] == 0L) {
	    ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
		     comm));
	    returnValue=MPI_ERR_COMM;
	    goto BarrierTag;
	}

    } // end OPT_CHECK_API_ARGS

    // get new contextID
    returnValue=communicators[comm]->getNewContextID(newComm);
    if( returnValue != MPI_SUCCESS ) {
	goto BarrierTag;
    }

    // collect all color, key pairs, so that groups can be created
    int pair[2];
    groupSize = communicators[comm]->localGroup->groupSize;
    allPairs=(int *)ulm_malloc(2 * sizeof(int) * groupSize);
    if(!allPairs) {
	ulm_err(("Error: ulm_comm_split: can''t allocate memory for allPairs\n"));
	returnValue=MPI_ERR_NO_SPACE;
	goto BarrierTag;
    }
    pair[0]=color;
    if( ! (pair[0]==MPI_UNDEFINED) ) {
	pair[1]=key;
    } else {
	pair[1]=MPI_UNDEFINED;
    }

    // gather all the infomation
    returnValue = ulm_gather(pair, 2, (ULMType_t *) MPI_INT, allPairs,
			     2, (ULMType_t *) MPI_INT, 0, comm);
    if(returnValue != ULM_SUCCESS) {
	returnValue=MPI_ERR_OTHER;
	goto BarrierTag;
    }
    returnValue = ulm_bcast(allPairs, 2 * sizeof(int) * groupSize,
                            (ULMType_t *)MPI_BYTE, 0, comm);
    if(returnValue != ULM_SUCCESS) {
	returnValue=MPI_ERR_OTHER;
	goto BarrierTag;
    }


    // if color is MPI_UNDEFINED, done - return
    if( color == MPI_UNDEFINED ) {
	*newComm=MPI_COMM_NULL;
	returnValue=MPI_SUCCESS;
	goto BarrierTag;
    }

    // if color is negative (and not MPI_UNDEFINED) return error
    if( color < 0 ) {
	returnValue=MPI_ERR_OTHER;
	goto BarrierTag;
    }

    // find how many processes have the same color value
    nMyColor=0;
    for( int proc=0 ; proc < communicators[comm]->localGroup->groupSize ;
	 proc++ ) {
	if( allPairs[2*proc] == color ) {
	    nMyColor++;
	}
    }

    // condense list
    list=(int *)ulm_malloc(sizeof(int)*nMyColor);
    if(!list) {
	ulm_err(("Error: ulm_comm_split: can''t allocate memory for list\n"));
	returnValue=MPI_ERR_NO_SPACE;
	goto BarrierTag;
    }
    cnt=0;
    for( int proc=0 ; proc < communicators[comm]->localGroup->groupSize ;
	 proc++ ) {
	if( allPairs[2*proc] == color ) {
	    list[cnt]=proc;
	    cnt++;
	}
    }
    listDone=(bool *)ulm_malloc(sizeof(bool)*nMyColor);
    if(!list) {
	ulm_err(("Error: ulm_comm_split: can''t allocate memory for listDene\n"));
	returnValue=MPI_ERR_NO_SPACE;
	goto BarrierTag;
    }
    for( int i=0 ; i < nMyColor ; i++ )
	listDone[i]=false;
    //
    // create sorted list (based on key values)
    sortedList=(int *)ulm_malloc(sizeof(int)*nMyColor);
    if(!sortedList) {
	ulm_err(("Error: ulm_comm_split: can''t allocate memory for sortedList\n"));
	returnValue=MPI_ERR_NO_SPACE;
	goto BarrierTag;
    }
    // loop over all processes
    for( int proc=0 ; proc < nMyColor ; proc++ ) {

        // find first available element
	int minElement=-1;
	for( int p=0 ; p < nMyColor ; p++ ) {
	    if( !listDone[p] ) {
		minElement=p;
		break;
	    }
	}
        // find smallest value
	int minValue=allPairs[2*list[minElement]+1];
	int min=minElement;
	for( int i=min ; i < nMyColor ; i++ ) {
	    if(listDone[i])
		continue;
	    if( minValue > allPairs[2*list[i]+1] ) {
		minValue=allPairs[2*list[i]+1];
		minElement=i;
	    }
	}
        // set sortedList
	sortedList[proc]=list[minElement];
	listDone[minElement]=true;
    } // end proc loop

    // create new group
    int newLocalGroup;
    returnValue=ulm_group_incl(communicators[comm]->localGroup->groupID,
			       nMyColor, sortedList, &newLocalGroup);
    if( returnValue != MPI_SUCCESS ) {
	goto BarrierTag;
    }

    // increment group counter
    grpPool.groups[newLocalGroup]->incrementRefCount(2);

    // create comunicator
    returnValue=ulm_communicator_alloc(*newComm,
                                       communicators[comm]->useThreads,newLocalGroup,newLocalGroup, false, 0,
                                       (int *)NULL, Communicator::INTRA_COMMUNICATOR, true, 1,
                                       communicators[comm]->useSharedMemForCollectives);


BarrierTag:
    int tmpreturnValue=returnValue;

    // block to make sure the new communicator is set up in all processes
    //   before  any data is send
    returnValue=ulm_barrier(comm);
    if( returnValue != ULM_SUCCESS ) {
	if( allPairs) {
	    ulm_free(allPairs);
	}
	if( sortedList) {
	    ulm_free(sortedList);
	}
	if( listDone) {
	    ulm_free(listDone);
	}
	if( list) {
	    ulm_free(list);
	}
	return returnValue;
    }

    // free temp resources
    if( allPairs) {
	ulm_free(allPairs);
    }
    if( sortedList) {
	ulm_free(sortedList);
    }
    if( listDone) {
	ulm_free(listDone);
    }
    if( list) {
	ulm_free(list);
    }

    returnValue=tmpreturnValue;
    return returnValue;
}
