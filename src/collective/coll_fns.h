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


#ifndef _COLL_FNS
#define _COLL_FNS

#include "queue/Communicator.h"

int exchangeStripeOfData(Communicator *commPtr, void *SharedBuffer,
                         list_t *hostExchangeList, size_t *dataToSendNow,
                         size_t *dataToRecvNow, int numHosts, int tag, int nExtra,
                         int extraOffset);

void copyDataFromSharedBufferMultipleStripesGather(
    Communicator *commPtr, void *RESTRICT_MACRO destinationBuffer,
    void *sharedBuffer, int *dataHostOrder, int recvcount, int numHosts,
    size_t *dataToSendThisStripe, int *currentLocalRank,
    size_t *currentRankBytesRead, ULMType_t *recvtype, int stripeID);

void copyDataFromSharedBufferMultipleStripesAllGather(
    Communicator *commPtr, void *RESTRICT_MACRO destinationBuffer,
    void *sharedBuffer, int *dataHostOrder, int recvcount, int numHosts,
    size_t *dataToSendThisStripe, int *currentLocalRank,
    size_t *currentRankBytesRead, ULMType_t *recvtype, int stripeID);

void copyDataFromSharedBufferGather1Stripe(
    Communicator *commPtr, void *destinationBuffer, void *sharedBuffer,
    int *dataHostOrder, int recvcount, int numHosts,
    size_t *dataToSendThisStripe, int *currentLocalRank,
    size_t *currentRankBytesRead, ULMType_t *recvtype);

void copyDataToSharedBufferMultipleStripesGather(
    size_t bytesToCopy, size_t bytesAlreadyCopied,
    size_t offsetIntoSharedBuffer, void *RESTRICT_MACRO sendbuf,
    void *RESTRICT_MACRO sharedBuffer, ULMType_t *sendtype);

void copyDataToSharedBufferMultipleStripes(size_t bytesToCopy, int sendCount,
                                           size_t bytesAlreadyCopied, size_t offsetIntoSharedBuffer, void *sendbuf,
                                           void *sharedBuffer, ULMType_t *sendtype);

void copyDataToSharedBuffer1Stripe(int currentStripe, void *sourceBuffer,
                                   void *sharedBuffer, int sendcount, ULMType_t *sendtype, Communicator *commPtr);

int ulm_gather_interhost(int comm, void *SharedBuffer, ulm_tree_t *treeData,
                         size_t *dataToSendNow, int root, int tag);

int ulm_scatter_interhost(int comm, void *dataBuffer, ulm_tree_t *treeData,
                          size_t dataPerProc, int root, int tag);

int ulm_scatterv_interhost(int comm, void *dataBuffer,
                           ulm_tree_t *treeData, size_t *dataPerHost, int root, int tag);

int ulm_bcast_interhost(void *buf, size_t count, ULMType_t *type, int root, int comm);


#endif /* _COLL_FNS */
