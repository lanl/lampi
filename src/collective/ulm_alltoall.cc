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
#include <math.h>
#include <string.h>
#include "internal/mpi.h"
#include "internal/malloc.h"

typedef struct group{
    int groupNumber;
    int groupSize;
    int* elementsOfGroup;
} group_t;

extern "C" int ulm_alltoall(void *sendbuf, int sendcount, ULMType_t *sendtype,
			    void *recvbuf, int recvcnt, ULMType_t *recvtype,
			    int comm)
{
    int		numberGroups= 1 ;
    int		i, group;
    int		placeInGroup;
    int		maxPlace;
    group_t*	groups;
    int		maxGroup;
    int		count= 0;
    int		procs;
    int		destProc;
    int		srcProc;
    int		destGroup;
    int		srcGroup;
    int		rc;
    int		tag;
    int		comm_size;
    int		self;
    ULMRequestHandle_t	recvRequest;
    ULMRequestHandle_t*	sendRequest;
    ULMStatus_t	recvStatus;
    ULMStatus_t	sendStatus;
    unsigned char*	buf_loc;

    rc = ulm_comm_size(comm, &comm_size);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_COMM;
    }
    rc = ulm_get_system_tag(comm, 1, &tag);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_INTERN;
    }

    sendRequest = (ULMRequestHandle_t *)ulm_malloc(sizeof(ULMRequestHandle_t) * comm_size);
    if (!sendRequest) {
	fprintf(stderr, "ulm_alltoall: unable to allocate send request array memory!\n");
	exit(-1);
    }

    if (comm_size > 100)
	numberGroups = 2;

    ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));


    maxPlace = (int)((comm_size/numberGroups) + 1);
    maxGroup =  numberGroups;
    groups = (group_t*)ulm_malloc(sizeof(group_t)*maxGroup);
    for (i=0;i<maxGroup;i++){
	groups[i].elementsOfGroup  = (int*)ulm_malloc(sizeof(int)*maxPlace);
	groups[i].groupSize = 0;
	groups[i].groupNumber	   = i;
	memset((void*)groups[i].elementsOfGroup, -9, maxPlace);
    }
    for (i=0;i<comm_size;i++){
	group=i%numberGroups;
	placeInGroup = (int)(i/numberGroups);
	groups[group].elementsOfGroup[placeInGroup]=i;
	groups[group].groupSize++;
    }

    srcGroup=(self%numberGroups);
    buf_loc = (unsigned char *) sendbuf;
    for (destGroup=srcGroup, count=0;count < maxGroup;)
    {
	for (procs = 0; procs < groups[destGroup].groupSize;procs++)
	{
	    destProc = groups[destGroup].elementsOfGroup[procs];
	    rc = ulm_isend(buf_loc+destProc*sendcount*sendtype->extent,
			   sendcount, sendtype, destProc, tag,
			   comm, &(sendRequest[procs]),  ULM_SEND_STANDARD);

	    if (rc != ULM_SUCCESS) break;
	}

	buf_loc = (unsigned char *) recvbuf;
	for (procs = 0; procs < groups[srcGroup].groupSize;procs++)
	{
	    srcProc = groups[srcGroup].elementsOfGroup[procs];
	    rc = ulm_irecv(buf_loc + srcProc*recvcnt*recvtype->extent,
			   recvcnt, recvtype, srcProc,
			   tag,  comm,  &recvRequest);

	    rc = ulm_wait(&recvRequest, &recvStatus);
	    if (rc != ULM_SUCCESS) break;


	}

	for (procs = 0; procs < groups[destGroup].groupSize;procs++)
	{
	    destProc = groups[destGroup].elementsOfGroup[procs];
	    rc = ulm_wait(&(sendRequest[procs]), &sendStatus);

	    if (rc != ULM_SUCCESS) break;
	}

	count++;
	srcGroup=(srcGroup-1)%maxGroup;
	if (srcGroup<0)
	    srcGroup = numberGroups-1;

	destGroup = (destGroup+1)%maxGroup;

    }

    ulm_free(sendRequest);
    for (i=0;i<maxGroup;i++){
	ulm_free(groups[i].elementsOfGroup);
    }
    ulm_free(groups);

    return rc;
}
