/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/



#include <stdio.h>
#include <math.h>
#include <string.h>
#include "internal/mpi.h"
#include "internal/malloc.h"

#ifndef GINGER_ALLTOALL 
#ifdef ENABLE_QSNET

/*
 * This is a simple implementation based on a sequence of gathers.
 *
 * We are using this for quadrics currently because more aggressive
 * algorithms can lead to resource exhaustion even on moderate numbers of
 * processes (> 32).
 *
 * Action items:
 * (a) Make quadrics path more robust to resource exhaustion
 * (b) Rethink the alltoall implementation which remains far from optimal
 */ 
extern "C" int ulm_alltoall(void *sendbuf, int sendcount, ULMType_t *sendtype,
			    void *recvbuf, int recvcount, ULMType_t *recvtype,
			    int comm)
{
    int nproc;
    int rc;
    int root;
    
    rc = ulm_comm_size(comm, &nproc);
    if (rc != ULM_SUCCESS) {
        return MPI_ERR_COMM;
    }

    for (root = 0; root < nproc; root++) {
        rc = ulm_gather((char*) sendbuf + root * sendcount * sendtype->extent,
                        sendcount, sendtype,
                        recvbuf, recvcount, recvtype,
                        root, comm);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
    }
    
    return ULM_SUCCESS;
}

#else

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
    ULMRequest_t	recvRequest;
    ULMRequest_t*	sendRequest;
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

    sendRequest = (ULMRequest_t *)ulm_malloc(sizeof(ULMRequest_t) * comm_size);
    if (!sendRequest) {
	ulm_err(("Error: Out of memory\n"));
	return MPI_ERR_INTERN;
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
    for (destGroup=srcGroup, count=0;count < maxGroup;)
    {
	    buf_loc = (unsigned char *) sendbuf;
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

#endif
#else

typedef struct index_table{
  int* dest;
  int* elements;
}index_t;

int getLog2 (int comm_size, int *power2, int *leftover ){
  int i;
  int log2;

  i = 1;
  log2 = 0;
  *leftover = 0;
  *power2 = 1;

  while ( i < comm_size ){
    i *= 2;
    log2++;
  }

  for (i=0;i<log2;i++) *power2 *= 2;

  if (*power2 != comm_size){
    log2 -= 1;
    *power2 /= 2;
    *leftover = comm_size - *power2;
  }

  return( log2 );
}

int createIndexTable(index_t *indices, int comm_size,
                     void *sendbuf, ULMType_t *sendtype, int log2)
{

  int i, j, k;
  int group;
  int dest2;

  /* Grab space for index tracking table */
  for (i=0;i<comm_size;i++){
    indices[i].dest     = (int*)malloc(sizeof(int)*log2);
    if (!indices[i].dest) 
      return ULM_ERR_OUT_OF_RESOURCE;

    indices[i].elements = (int*)malloc(sizeof(int)*log2);
    if (!indices[i].elements) 
      return ULM_ERR_OUT_OF_RESOURCE;
  }

  /****
   * Fill out the index table. I guess some explaining about that table
   * needs to go into here.  Here it goes:
   *
   *  indices[process].dest[loop] contains the destination process.
   *  indices[process].elements[loop] contains the number of elements to xfr.
   *
   *  processe is 0 to comm_size
   *  loop     is the n in 2^n number of processes (minus 1 for the index)
   *
   *  Choosing an easy and smallish number of process for an example(8)
   *  The table end ups looking like:
   *
   *       Destination:            Elements:
   *      Loops-->   0  1  2         0  1  2
   *      procs| [0] 1  2  4     [0] 1  2  4
   *           | [1] 0  3  5     [1] 1  2  4
   *           v [2] 3  0  6     [2] 1  2  4
   *             [3] 2  1  7     [3] 1  2  4
   *             [4] 5  6  0     [4] 1  2  4
   *             [5] 4  7  1     [5] 1  2  4
   *             [6] 7  4  2     [6] 1  2  4
   *             [7] 6  5  3     [7] 1  2  4
   *****/
  k = 1;
  dest2 = 0;
  group = 1;
  for (i=0;i<log2;i++){
    for (j=0;j<comm_size;j++){
      if ( k < (group*2) ){
        if (k<=group){
          indices[j].dest[i]     = j + group; 
          indices[j].elements[i] = group;
          dest2 = j + (group-1);
        }
        else{
          dest2 = j - group;
          indices[j].dest[i]     = dest2; 
          indices[j].elements[i] = group;
          dest2++;
        }
        k++;
      }
      else {
        indices[j].dest[i]     = dest2;
        indices[j].elements[i] = group;
        k=1;
      }
    }
    group *=2;
  }
  return ULM_SUCCESS;
}/* createIndexTable */

int exchangeByLog2 (void *sendbuf, int sendcount, ULMType_t *sendtype,
                    void *recvbuf, int recvcnt, ULMType_t *recvtype,
                    int comm, int comm_size, int self, 
                    int rc, int tag, int log2, index_t *indices)
{
  int           i,j;
  int           count= 0;
  int           destProc;
  int           skip;
  int           flip;
  unsigned char* buf_loc;
  unsigned char* temp;
  ULMRequest_t  recvRequest;
  ULMRequest_t  sendRequest;
  ULMStatus_t   recvStatus;
  ULMStatus_t   sendStatus;


  temp = (unsigned char*)ulm_malloc(sendtype->extent*comm_size);
  if (!temp) 
    return ULM_ERR_OUT_OF_RESOURCE;

  /* If all Okay, pass the data in a 2^n segments */
  memcpy( recvbuf, sendbuf, sendtype->extent*comm_size);
  memcpy( temp, sendbuf, sendtype->extent*comm_size);

  for (i=0;i<log2;i++){
    j=0;
    skip = 0;
    flip = 0;

    while ( j < comm_size){

      buf_loc = temp;
    
      destProc = indices[self].dest[i];
      count = indices[self].elements[i];
      
      if (count == 1){
        if ((self%2)==0)
          skip = j + 1; 
        else 
          skip = j;
      }
      else {

        flip = self%(count*2);

        if ( flip < (count)){
          skip = j + count;
        }
        else {
          skip = j;
        }
      }

      rc = ulm_isend(buf_loc+(skip*(sendtype->extent)),
                     count, sendtype, destProc, tag,
                     comm, &sendRequest,  ULM_SEND_STANDARD);
    
      if (rc != ULM_SUCCESS) break;

      /* set up the receive */
      buf_loc = (unsigned char *) recvbuf;

      rc = ulm_irecv(buf_loc + (skip*(recvtype->extent)),
                     count, recvtype, destProc,
                     tag,  comm,  &recvRequest);

      rc = ulm_wait(&recvRequest, &recvStatus);
      if (rc != ULM_SUCCESS) break;

      rc = ulm_wait(&sendRequest, &sendStatus);      
      if (rc != ULM_SUCCESS) break;

      j = j + 2*count;
    }

    memcpy( temp, recvbuf, sendtype->extent*comm_size);

  }
  ulm_free( temp );
  return(rc);
} /* exchangeByLog2 */

int exchangeFakeLog2 (void *sendbuf, int sendcount, ULMType_t *sendtype,
                      void *recvbuf, int recvcnt, ULMType_t *recvtype,
                      void *sendbuf_extra, void *recvbuf_extra, 
                      int comm, int comm_size, int modifiedSelf, 
                      int rc, int rc_extra, int tag, int log2, int power2, 
                      int extra, index_t *indices)
{
  int           i,j;
  int           count= 0;

  int           destProc;
  int           skip;
  int           flip;
  unsigned char* buf_loc;
  unsigned char* temp;
  ULMRequest_t  recvRequest;
  ULMRequest_t  sendRequest;
  ULMStatus_t   recvStatus;
  ULMStatus_t   sendStatus;

  int           destProc_extra;
  int           skip_extra;
  int           flip_extra;
  unsigned char* buf_loc_extra;
  unsigned char* temp_extra;
  ULMRequest_t  recvRequest_extra;
  ULMRequest_t  sendRequest_extra;
  ULMStatus_t   recvStatus_extra;
  ULMStatus_t   sendStatus_extra;

  temp = (unsigned char*)ulm_malloc((sendtype->extent)*power2);
  if (!temp) 
    return ULM_ERR_OUT_OF_RESOURCE;

  /* If all Okay, pass the data in a 2^n segments */
  memcpy( recvbuf, sendbuf, sendtype->extent*comm_size);
  memcpy( temp, sendbuf, sendtype->extent*comm_size);

  if (extra){
    temp_extra = (unsigned char*)ulm_malloc((sendtype->extent)*power2);
    if (!temp_extra) 
      return ULM_ERR_OUT_OF_RESOURCE;

    memcpy( recvbuf_extra, sendbuf, sendtype->extent*comm_size);
    memcpy( temp_extra, sendbuf_extra, (sendtype->extent)*comm_size);

  }

  for (i=0;i<log2;i++){
    j=0;
    skip = 0;
    flip = 0;

    if (extra) {
      skip_extra = 0;
      flip_extra = 0;
    }

    while ( j < power2){

      buf_loc = temp;
    
      destProc = indices[modifiedSelf].dest[i];
      count = indices[modifiedSelf].elements[i];

      if (extra){
        buf_loc_extra = temp_extra;
        
        destProc_extra = indices[modifiedSelf+comm_size].dest[i];
      }

      if (count == 1){
        if ((modifiedSelf%2)==0)
          skip = j + 1; 
        else 
          skip = j;
        if (extra){
          if (((modifiedSelf+comm_size)%2)==0)
            skip_extra = j + 1; 
          else 
            skip_extra = j;
        }
      }
      else {

        flip = modifiedSelf%(count*2);
        if (extra)
          flip_extra = (modifiedSelf+comm_size)%(count*2);

        if ( flip < (count)){
          skip = j + count;
        }
        else {
          skip = j;
        }
        if (extra){
          if ( flip_extra < (count)){
            skip_extra = j + count;
          }
          else {
            skip_extra = j;
          }
        }
      }

      if (destProc >= comm_size){
        destProc -= comm_size;
      }

      if (extra)
        if (destProc_extra >= comm_size){
          destProc_extra -= comm_size;
        }

      if (extra) {
        rc_extra = ulm_isend(buf_loc_extra+(skip_extra*(sendtype->extent)),
                             count, sendtype, destProc_extra, tag,
                             comm, &sendRequest_extra,  ULM_SEND_STANDARD);
    
        if (rc_extra != ULM_SUCCESS) break;
      }
      rc = ulm_isend(buf_loc+(skip*(sendtype->extent)),
                     count, sendtype, destProc, tag,
                     comm, &sendRequest,  ULM_SEND_STANDARD);
    
      if (rc != ULM_SUCCESS) break;

      /* set up the receive */
      buf_loc = (unsigned char *) recvbuf;
      if (extra)
        buf_loc_extra = (unsigned char *) recvbuf_extra;

      if (extra) {
        rc_extra = ulm_irecv(buf_loc_extra + (skip_extra*(recvtype->extent)),
                       count, recvtype, destProc_extra,
                       tag,  comm,  &recvRequest_extra);

        rc_extra = ulm_wait(&recvRequest_extra, &recvStatus_extra);
        if (rc_extra != ULM_SUCCESS) break;

        rc_extra = ulm_wait(&sendRequest_extra, &sendStatus_extra);      
        if (rc_extra != ULM_SUCCESS) break;
      }
      rc = ulm_irecv(buf_loc + (skip*(recvtype->extent)),
                     count, recvtype, destProc,
                     tag,  comm,  &recvRequest);

      rc = ulm_wait(&recvRequest, &recvStatus);
      if (rc != ULM_SUCCESS) break;

      rc = ulm_wait(&sendRequest, &sendStatus);      
      if (rc != ULM_SUCCESS) break;

      j = j + 2*count;
    }

    memcpy( temp, recvbuf, sendtype->extent*power2);
    if (extra)
      memcpy( temp_extra, recvbuf_extra, sendtype->extent*power2);

  }

  ulm_free( temp );

  if (extra){
    ulm_free( temp_extra );
    if (rc == rc_extra) return (rc);
    else return (rc || rc_extra);
  }
  return(rc);
} /* exchangeFakeLog2 */

int exByProcess (void *sendbuf, int sendcount, ULMType_t *sendtype,
                 void *recvbuf, int recvcnt, ULMType_t *recvtype,
                 int comm, int comm_size, int power2, int self, int rc, int tag)
{
  int           destProc;
  unsigned char* buf_loc;
  ULMRequest_t  recvRequest;
  ULMRequest_t  sendRequest;
  ULMStatus_t   recvStatus;
  ULMStatus_t   sendStatus;

  if (self >= power2){

    for (destProc=0;destProc<comm_size;destProc++){
      buf_loc = (unsigned char *)sendbuf;
    
      if ( self != destProc){

        rc = ulm_isend(buf_loc+destProc*(sendtype->extent),
                       1, sendtype, destProc, tag,
                       comm, &sendRequest,  ULM_SEND_STANDARD);
    
        if (rc != ULM_SUCCESS) break;

        /* set up the receive */
        buf_loc = (unsigned char *) recvbuf;

        rc = ulm_irecv(buf_loc + destProc*(recvtype->extent),
                       1, recvtype, destProc,
                       tag,  comm,  &recvRequest);

        rc = ulm_wait(&recvRequest, &recvStatus);
        if (rc != ULM_SUCCESS) break;

        rc = ulm_wait(&sendRequest, &sendStatus);      
        if (rc != ULM_SUCCESS) break;
      }
    }
  }
  else{
    for (destProc=power2;destProc<comm_size;destProc++){
      
      if (self < power2 ){
        buf_loc = (unsigned char *)sendbuf;
    
        rc = ulm_isend(buf_loc+destProc*(sendtype->extent),
                       1, sendtype, destProc, tag,
                       comm, &sendRequest,  ULM_SEND_STANDARD);
    
        if (rc != ULM_SUCCESS) break;

        /* set up the receive */
        buf_loc = (unsigned char *) recvbuf;

        rc = ulm_irecv(buf_loc +destProc*(recvtype->extent),
                       1, recvtype, destProc,
                       tag,  comm,  &recvRequest);

        rc = ulm_wait(&recvRequest, &recvStatus);
        if (rc != ULM_SUCCESS) break;

        rc = ulm_wait(&sendRequest, &sendStatus);      
        if (rc != ULM_SUCCESS) break;
      }
    }
  }
  return(rc);
} /* exchangeByProcess */


extern "C" int ulm_alltoall(void *sendbuf, int sendcount, ULMType_t *sendtype,
                            void *recvbuf, int recvcnt, ULMType_t *recvtype,
                            int comm)
{
  int           rc;
  int           tag;
  int           comm_size;
  int           self;
  int           leftover;
  int           log2;
  int           power2;
  index_t*      indices;

  rc = ulm_comm_size(comm, &comm_size);
  if (rc != ULM_SUCCESS) {
    return MPI_ERR_COMM;
  }
  rc = ulm_get_system_tag(comm, 1, &tag);
  if (rc != ULM_SUCCESS) {
    return MPI_ERR_INTERN;
  }

  ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));

  /***/
  /* Do we have a power 2? */
  log2 = getLog2 (comm_size, &power2, &leftover);

  /* There is a power of two, so it's easy */
  if ( leftover == 0){

    indices = (index_t*)ulm_malloc(sizeof(index_t)*comm_size);
    if (!indices) 
      return ULM_ERR_OUT_OF_RESOURCE;
    
    /* set up the index table */
    if (createIndexTable(indices, comm_size,
                         sendbuf, sendtype, log2) != ULM_SUCCESS)
      return MPI_ERR_COMM;


    rc = exchangeByLog2 (sendbuf, sendcount, sendtype,
                         recvbuf, recvcnt, recvtype,
                         comm, comm_size, self, rc, tag, 
                         log2, indices);
  }

  /* Create Dummy processes to the next power of 2
   */
  else {
    unsigned char *send_extra;
    unsigned char *recv_extra;
    unsigned char *recv_larger;

    log2++;
    power2 *= 2;

    indices = (index_t*)ulm_malloc(sizeof(index_t)*power2);
    if (!indices) 
      return ULM_ERR_OUT_OF_RESOURCE;

    /* set up the index table */
    if (createIndexTable(indices, power2,
                         sendbuf, sendtype, log2) != ULM_SUCCESS)
      return MPI_ERR_COMM;

      recv_larger = (unsigned char*)ulm_malloc((sendtype->extent)*power2);
      if (!recv_larger) 
        return ULM_ERR_OUT_OF_RESOURCE;

    if ( self < (power2-comm_size)){

      send_extra = (unsigned char*)ulm_malloc((sendtype->extent)*power2);
      if (!send_extra) 
        return ULM_ERR_OUT_OF_RESOURCE;

      recv_extra = (unsigned char*)ulm_malloc((sendtype->extent)*power2);
      if (!recv_extra) 
        return ULM_ERR_OUT_OF_RESOURCE;

      memcpy( send_extra, sendbuf, (sendtype->extent)*comm_size);

      int rc_extra;
      rc = exchangeFakeLog2 (sendbuf, sendcount, sendtype,
                             recv_larger, recvcnt, recvtype,
                             send_extra, recv_extra,
                             comm, comm_size, self, rc, rc_extra, tag, 
                             log2, power2, 1, indices);
      ulm_free(send_extra);
      ulm_free(recv_extra);

    }
    else{

      void *send_fake, *recv_fake;
      
      rc = exchangeFakeLog2 (sendbuf, sendcount, sendtype,
                             recv_larger, recvcnt, recvtype,
                             send_fake, recv_fake,
                             comm, comm_size, self, rc, rc, tag, 
                             log2, power2, 0, indices);      

    }
    memcpy( recvbuf, recv_larger, (sendtype->extent)*comm_size);
    ulm_free(recv_larger);
  }

  /* clean-up */
  ulm_free(indices);
    
  return rc;
}

#endif
