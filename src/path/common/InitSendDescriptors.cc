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
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "internal/log.h"
#include "internal/constants.h"
#include "internal/malloc.h"
#include "client/ULMClient.h"
#include "queue/globals.h" /* ShareMemDescPool */
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "queue/globals.h"
#include "path/common/InitSendDescriptors.h"

// instantiate globals in net/common/InitSendDescriptors.h
FreeListPrivate_t<BaseSendDesc_t> _ulm_SendDescriptors;
ssize_t _ulm_nSendDescPages = -1;
ssize_t _ulm_maxPgsIn1SendDescList = -1;
ssize_t _ulm_minPgsIn1SendDescList = -1;
long _ulm_maxSendDescRetries = 1000;
bool _ulm_sendDescAbortWhenNoResource = true;

/*
 *  Initialize the send message descriptors
 */
void InitSendDescriptors(int NumLocalProcs)
{
    long minPagesPerList = 1;
    long nPagesPerList = 100;
    long maxPagesPerList = _ulm_maxPgsIn1SendDescList;
    ssize_t pageSize = SMPPAGESIZE;
    ssize_t eleSize = sizeof(BaseSendDesc_t);
    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = 1;
    int retryForMoreResources = 1;
    int *memAffinityPool = (int *) ulm_malloc(sizeof(int) * NumLocalProcs);
    if (!memAffinityPool) {
        ulm_exit((-1, "Unable to allocate space for memAffinityPool\n"));
    }
    // fill in memory affinity index
    for (int i = 0; i < nFreeLists; i++) {
        memAffinityPool[i] = i;
    }
    MemoryPoolPrivate_t *inputPool = NULL;

    int retVal =
        _ulm_SendDescriptors.Init(nFreeLists, nPagesPerList, poolChunkSize,
                                  pageSize, eleSize,
                                  minPagesPerList, maxPagesPerList,
                                  _ulm_maxSendDescRetries,
                                  " Send Descriptors ",
                                  retryForMoreResources, memAffinityPool,
                                  enforceAffinity(), inputPool,
                                  _ulm_sendDescAbortWhenNoResource);

    // clean up
    ulm_free(memAffinityPool);

    if (retVal) {
        ulm_exit((-1,
                  "FreeLists::Init Unable to initialize send "
                  "descriptor pool\n"));
    }
}
