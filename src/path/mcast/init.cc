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

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/options.h"
#include "internal/state.h"
#include "internal/types.h"
#include "path/mcast/localcollFrag.h"
#include "path/mcast/state.h"
#include "path/sharedmem/SMPDev.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"

// pool for collective frags - resides in process shared memory.  All
// structures are initialized before the fork() and are not modified
// there after.
FreeLists<DoubleLinkList, LocalCollFragDesc, int, MMAP_SHARED_PROT,
	  MMAP_SHARED_FLAGS, MMAP_SHARED_FLAGS> LocalCollFragDescriptors;

/* Initialize the Out of Order SMP shared memory descriptors */
void InitCollSharedMemDescriptors(int NumLocalProcs)
{
    // initialize pool of collective frag descriptors
    long minPagesPerList = 1;
    long nPagesPerList = 8;
    long maxPagesPerList = maxPgsIn1SMPCollFragDescList;
    ssize_t pageSize = SMPPAGESIZE;
    ssize_t eleSize = sizeof(LocalCollFragDesc);
    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = NumLocalProcs;
    int retryForMoreResources = 0;
    int *memAffinityPool = (int *) ulm_malloc(sizeof(int) * NumLocalProcs);
    // fill in memory affinity index
    for (int i = 0; i < nFreeLists; i++) {
        memAffinityPool[i] = i;
    }

    int retVal =
        LocalCollFragDescriptors.Init(nFreeLists, nPagesPerList,
                                      poolChunkSize, pageSize, eleSize,
                                      minPagesPerList, maxPagesPerList,
                                      maxSMPCollFragDescRetries,
                                      "Collective frag descriptors ",
                                      retryForMoreResources,
                                      memAffinityPool,
                                      enforceMemoryPolicy(),
                                      ShareMemDescPool,
                                      SMPCollFragDescAbortWhenNoResource);
    if (retVal) {
        ulm_exit((-1, "FreeLists::Init Unable to initialize Collective "
                  "descriptor pool\n"));
    }
    // clean up 
    ulm_free(memAffinityPool);
}
