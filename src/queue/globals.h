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

#ifndef _QUEUEGLOBALS
#define _QUEUEGLOBALS

#include "queue/Communicator.h"
#include "queue/Group.h"
#include "queue/SharedMemForCollective.h"
#include "queue/barrier.h"
#include "queue/contextID.h"
#include "util/DblLinkList.h"
#include "include/internal/mmap_params.h"
#include "init/init.h"
#include "internal/buffer.h"
#include "internal/types.h"
#include "mem/FixedSharedMemPool.h"
#include "mem/FreeLists.h"
#include "mem/MemoryPool.h"

#ifdef RELIABILITY_ON
#include "queue/ReliabilityInfo.h"
#endif

////////////////////////////////////////////////////////////////////////
// Instantiate global variables

#ifdef ULM_INSTANTIATE_GLOBALS

// shared memory pool for use before fork
FixedSharedMemPool SharedMemoryPools;

// control structure for shared memory collective optimization
CollectiveSMBufferPool_t *SharedMemForCollective;

// list of frags for collective operations
SharedMemDblLinkList **_ulm_CommunicatorRecvFrags;

// sorted incomplete sends
ProcessPrivateMemDblLinkList IncompletePostedSends;

// sorted unacked sends
ProcessPrivateMemDblLinkList UnackedPostedSends;

// acks that still need to be sent
ProcessPrivateMemDblLinkList UnprocessedAcks;

// pool of contiguous Ireceive descriptors
FreeListPrivate_t <RecvDesc_t> IrecvDescPool;
ssize_t maxPgsIn1IRecvDescDescList = -1;
ssize_t minPgsIn1IRecvDescDescList = -1;
long maxIRecvDescRetries = 1000;
bool irecvDescDescAbortWhenNoResource = true;

// pool of RequestDesc_ts
FreeListPrivate_t <RequestDesc_t> ulmRequestDescPool;

// list of request objects that are potentially still being used
ProcessPrivateMemDblLinkList _ulm_incompleteRequests;

// communictor management
//

// uppper limit on number of communicators
int maxCommunicatorInstances = 200;

// number of active communicators
int nCommunicatorInstancesInUse;

// list of communicators in use
int *activeCommunicators;

// array of pointers to list Communicator indexed by
//  contextID.  A value of -1L indicates group inactive.
Communicator **communicators;
Locks communicatorsLock;

//! lenght of communicators array
int communicatorsArrayLen=10000;

// number of elements by which to increase the size of the
//  array communicators
int nIncreaseCommunicatorsArray;

//
//  attribute management
//
attributePool_t attribPool;

//
// group management
//
// pool of group objects - this resides in process private memory
//
groupPool grpPool;

//
// group pool increment
//
int nIncreaseGroupArray;

FreeListPrivate_t <ptrLink_t> pointerPool;

ssize_t npointerPoolPages = -1;
ssize_t maxPgsIn1pointerPool = -1;
ssize_t minPgsIn1pointerPool = -1;
long maxpointerPoolRetries = 100;
bool pointerPoolAbortWhenNoResource = true;

#ifdef RELIABILITY_ON
ReliabilityInfo *reliabilityInfo = 0;
// last dclock() time CheckForRetransmits() was called
// this way we can only check every MIN_RETRANS_TIME period
double lastCheckForRetransmits;
#endif

#else 

////////////////////////////////////////////////////////////////////////
// Declare global variables

// shared memory pool for use before fork
extern FixedSharedMemPool SharedMemoryPools;

// control structure for shared memory collective optimization
extern CollectiveSMBufferPool_t *SharedMemForCollective;

// resides is shared memory
extern SharedMemDblLinkList **_ulm_CommunicatorRecvFrags;

// sorted incomplete sends
extern ProcessPrivateMemDblLinkList IncompletePostedSends;

// sorted unacked sends
extern ProcessPrivateMemDblLinkList UnackedPostedSends;

// acks that still need to be sent
extern ProcessPrivateMemDblLinkList UnprocessedAcks;

// pool of IReceive descriptors
extern FreeListPrivate_t <RecvDesc_t> IrecvDescPool;

extern ssize_t maxPgsIn1IRecvDescDescList;
extern ssize_t minPgsIn1IRecvDescDescList;
extern long maxIRecvDescRetries;
extern bool irecvDescDescAbortWhenNoResource;

// pool of RequestDesc_ts
extern FreeListPrivate_t <RequestDesc_t> ulmRequestDescPool;

// list of request objects that are potentially still being used
extern ProcessPrivateMemDblLinkList _ulm_incompleteRequests;

// uppper limit on number of communicators
extern int maxCommunicatorInstances;

// number of active communicators
extern int nCommunicatorInstancesInUse;

// list of communicators in use
extern int *activeCommunicators;

// array of pointers to list Communicator indexed by
//  contextID.  A value of 0L indicates group inactive.

extern Communicator **communicators;
extern Locks communicatorsLock;

// lenght of communicators array
extern int communicatorsArrayLen;

// number of elements by which to increase the size of the
//  array communicators
extern int nIncreaseCommunicatorsArray;

//
//  attribute management
//
extern attributePool_t attribPool;

//
// group management
//
// pool of group objects - this resides in process private memory
//
extern groupPool grpPool;

//
// group pool increment
//
extern int nIncreaseGroupArray;

// double link list of pointers pool
extern FreeListPrivate_t <ptrLink_t> pointerPool;

extern ssize_t npointerPoolPages;
extern ssize_t maxPgsIn1pointerPool;
extern ssize_t minPgsIn1pointerPool;
extern long maxpointerPoolRetries;
extern bool pointerPoolAbortWhenNoResource;

// pool for SW SMP barrier data structures
extern SWBarrierPool swBarrier;

#ifdef RELIABILITY_ON
extern ReliabilityInfo *reliabilityInfo;
extern double lastCheckForRetransmits;
#endif

// per-proc shared memory pool for use before the fork (will apply
// memory affinity to each pool
extern FixedSharedMemPool PerProcSharedMemoryPools;

// shared memory pool for descriptors
extern MemoryPoolShared_t *ShareMemDescPool;

// mumber of bytes for shared memory descriptor pool - ShareMemDescPool
extern ssize_t bytesPerProcess;

// shared memory pool for larger objects - pool chunks will be larger for
// more effcient allocation
extern MemoryPoolShared_t *largeShareMemDescPool;

// mumber of bytes for shared memory descriptor pool - ShareMemDescPool
extern ssize_t largePoolBytesPerProcess;

#endif                          //ULM_INSTANTIATE_GLOBALS


////////////////////////////////////////////////////////////////////////
// SGI NUMA specific resources

#if defined(NUMA) && defined(__mips)
#include "os/IRIX/acquire.h"
extern acquire *ULMai;
extern request *ULMreq;
extern int nCpPNode;
extern bool useRsrcAffinity;
extern bool useDfltAffinity;
extern bool affinMandatory;
#endif /* NUMA and __mips*/


////////////////////////////////////////////////////////////////////////
// prototypes

int push_frags_into_network(double timeNow);
void CheckForAckedMessages(double timeNow);
int CheckForRetransmits();
void SendUnsentAcks();

//
//  get new group element
//
int getGroupElement(int *slotIndex);

//
// create a new group with the list of global ranks
//
int setupNewGroupObject(int grpSize, int *listOfRanks, int *groupIndex);
int peerExchange(int peerComm, int localComm, int localLeader,
                 int remoteLeader, int tag,
                 void *sendBuff, ssize_t lenSendBuff,
                 void *recvBuff, ssize_t lenRecvBuff,
                 ULMStatus_t * recvStatus);

#endif                          /* !_QUEUEGLOBALS */
