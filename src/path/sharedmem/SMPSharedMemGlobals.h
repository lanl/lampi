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

#ifndef _SMPSHAREDMEMGLOBALS
#define _SMPSHAREDMEMGLOBALS

#include "mem/FreeLists.h"
#include "include/internal/mmap_params.h"
#include "util/cbQueue.h"

#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/sharedmem/sendInfo.h"
#endif

#include "path/sharedmem/SMPDev.h"

#define IO_SOURCEALLOCATED   1         // library memory allocated for send size

// get payload buffer from SMPSharedMem_logical_dev_t
void * allocPayloadBuffer(SMPSharedMem_logical_dev_t *dev,
                          unsigned long length, int *errorCode, int memPoolIndex);

// upper limit on number of pages per forked proc used for on-SMP
//   messaging
extern int NSMPSharedMemPagesPerProc;

// size of largest bucket (for an optimization)
extern int SizeLargestSMPSharedMemBucket;

// pool for isend headers
extern ssize_t nSMPISendDescPages;
extern ssize_t maxPgsIn1SMPISendDescList;
extern ssize_t minPgsIn1SMPISendDescList;
extern long maxSMPISendDescRetries;
extern bool SMPISendDescAbortWhenNoResource;

// pool for recv frags - resides in process shared memory.  All structures
// are initialized before the fork() and are not modified there after.
extern ssize_t nSMPRecvDescPages;
extern ssize_t maxPgsIn1SMPRecvDescList;
extern ssize_t minPgsIn1SMPRecvDescList;
extern long maxSMPRecvDescRetries;
extern bool SMPRecvDescAbortWhenNoResource;

//! SMP send descriptor list
extern FreeListShared_t <sharedMemData_t> SMPSendDescs;

//! SMP frag descriptor list
extern FreeListShared_t <SMPSecondFragDesc_t> SMPFragPool;

//! first frags for which the payload buffers are not yet ready
extern ProcessPrivateMemDblLinkList firstFrags;

extern cbQueue<SMPFragDesc_t *, MMAP_SHARED_FLAGS, MMAP_SHARED_FLAGS>
 ***SharedMemIncomingFrags;
extern SharedMemDblLinkList **SMPSendsToPost;
extern SharedMemDblLinkList **SMPMatchedFrags;
extern ProcessPrivateMemDblLinkList IncompletePostedSMPSends;
extern ProcessPrivateMemDblLinkList UnackedPostedSMPSends;


#endif /* _SMPSHAREDMEMGLOBALS */
