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

/*
 * Control structures for the shared memory area that used in the
 * optimized collectives
 */

#ifndef _SHAREDCOLL
#define _SHAREDCOLL

#include "util/Lock.h"
#include "internal/system.h"

#define MIN_COLL_MEM_PER_PROC           CACHE_ALIGNMENT
#define MAX_COMMUNICATOR_SHARED_OBJECTS 80
#define MIN_COLL_SHARED_WORKAREA        524288
#define N_COLLCTL_PER_COMM              16

/* some flagElement flag values */
#define READY_TO_READ                   1
#define OK_TO_READ                      2

struct flagElement_t {
    int flag;
    char padding[CACHE_ALIGNMENT - sizeof(int)];
};

struct CollectiveSMBuffer_t {
    /* current collective's operation tag */
    volatile long long tag;
    /* number of ranks that have attached to this collective descriptor */
    volatile int allocRefCount;
    /* number of ranks that have freed this collective descriptor */
    volatile int freeRefCount;
    /* flag - used instead of barrier where only 1 proc will ever set it */
    volatile int flag;
    /* largest size of shared memory segment - size actually allocated */
    size_t max_length;
    /* pointer to shared memory segement */
    void *RESTRICT_MACRO mem;
    /* array of "flags" - in shared memory */
    flagElement_t *controlFlags;
    /* lock for this control structure */
    Locks lock;
};

struct CollectiveSMBufferArray_t {
    /* pool of CollectiveSMBuffer_t elements */
    CollectiveSMBuffer_t collDescriptor[N_COLLCTL_PER_COMM];
    /* flag indicating if this "pool" is free */
    volatile int inUse;
    /* context id of communicator using this pool */
    volatile int contextID;
    /* local comm root */
    volatile int localCommRoot;
    /* number of ranks that have freed this "pool" */
    volatile int nFreed;
};

struct CollectiveSMBufferPool_t {
    /* pool of CollectiveSMBufferArray_t elements */
    CollectiveSMBufferArray_t collCtlData[MAX_COMMUNICATOR_SHARED_OBJECTS];
    /* lock controling access to these shared resources */
    Locks lock;
};

#endif
