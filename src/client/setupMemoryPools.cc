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
#include <new>

#include "queue/globals.h"
#include "client/daemon.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/system.h"
#include "internal/types.h"
#include "mem/MemoryPool.h"
#include "ulm/ulm.h"

ssize_t bytesPerProcess;

MemoryPoolShared_t *ShareMemDescPool;

// shared memory pool for larger objects - pool chunks will be larger for
//   more effcient allocation
MemoryPoolShared_t *largeShareMemDescPool;

// mumber of bytes for shared memory descriptor pool - ShareMemDescPool
ssize_t largePoolBytesPerProcess;

// setup memory pools
void setupMemoryPools()
{
    size_t lenToAlloc = sizeof(MemoryPoolShared_t);
    ShareMemDescPool =
        (MemoryPoolShared_t *) 
        SharedMemoryPools.getMemorySegment(lenToAlloc,
                                           CACHE_ALIGNMENT);
    if (!ShareMemDescPool) {
        ulm_exit(("Error: initializing  ShareMemDescPool memory pool\n"));
    }
    // run constructor
    new(ShareMemDescPool) MemoryPoolShared_t;

    // setup pool for shared memory descriptors
    ssize_t bytesToAllocate = bytesPerProcess * local_nprocs();
    if (bytesToAllocate <= 0) {
        ulm_exit(("Error: allocating ShareMemDescPool.  "
                  "Bytes requested :: %ld (bytesPerProcess %ld local processes %d)\n",
                  bytesToAllocate, bytesPerProcess, local_nprocs()));
    }
    // allocate memory
    ssize_t PoolChunkSize = 2 * getpagesize();
    int errorCode = ShareMemDescPool->
        Init(bytesToAllocate, bytesToAllocate, PoolChunkSize,
             getpagesize());
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: initializing  ShareMemDescPool memory pool\n"));
    }

    lenToAlloc = sizeof(MemoryPoolShared_t);
    largeShareMemDescPool =
        (MemoryPoolShared_t *)
        SharedMemoryPools.getMemorySegment(lenToAlloc,
                                           CACHE_ALIGNMENT);
    if (!largeShareMemDescPool) {
        ulm_exit(("Error: initializing  ShareMemDescPool memory pool\n"));
    }
    // run constructor
    new(largeShareMemDescPool) MemoryPoolShared_t;

    // setup pool for shared memory descriptors
    bytesToAllocate = largePoolBytesPerProcess * local_nprocs();
    if (bytesToAllocate <= 0) {
        ulm_exit(("Error: allocating largeShareMemDescPool.  "
                  "Bytes reqested :: %ld\n, ", bytesToAllocate));
    }
    // allocate memory
    PoolChunkSize = 20 * getpagesize();
    errorCode = largeShareMemDescPool->
        Init(bytesToAllocate, bytesToAllocate, PoolChunkSize,
             getpagesize());
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: initializing  largeShareMemDescPool "
                  "memory pool\n"));
    }
}
