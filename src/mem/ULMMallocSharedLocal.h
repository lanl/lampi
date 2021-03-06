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

#ifndef _ULMMALLOCSHAREDLOCAL
#define _ULMMALLOCSHAREDLOCAL

// new this for placement new
#include <new>
#include <stdlib.h>
#include <assert.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "ulm/ulm.h"
#include "util/Lock.h"
#include "mem/ULMMemoryBuckets.h"
#include "mem/ULMPool.h"

/* Allocator for managing local shared memory.  The memory pool is
 * assumed to be allocated before the fork, and not growable afterwards.
 */
template <int Log2MaxPoolSize, int Log2PoolChunkSize, 
          int NumberOfBuckets, int Log2PageSize, 
          long MaxStackElements, int MemProt, int MemFlags, 
          bool ZeroAllocationBase>
class ULMMallocSharedLocal 
{
public:
    //! C'tor
    ULMMallocSharedLocal(ULMMemoryPool * sp = 0) {
        // create the shared memory pool, if one is not specified in the
        // constructor
        if (!sp) {
            SharedPool = (ULMMemoryPool *) ZeroAlloc(sizeof(ULMMemoryPool),
                                                     MemProt, MemFlags);
            new(SharedPool) ULMMemoryPool(Log2MaxPoolSize,
                                          Log2PoolChunkSize, Log2PageSize,
                                          MemProt, MemFlags, 0, true);
            ownsSharedPool = true;
        } else {
            SharedPool = sp;
            ownsSharedPool = false;
        }
    }

    //! D'tor, frees the Shared Memory pool if the object owns it
    ~ULMMallocSharedLocal() {
        // handle case when we own the shared pool
        if (ownsSharedPool && SharedPool != 0) {
            // Clean-up ULMMemoryPool
            SharedPool->DeletePool();
            // free the memory
            int retval = munmap(SharedPool, sizeof(ULMMemoryPool));
            if (retval == -1) {
                ulm_err(("Error: unmapping ULMMemoryPool.\n"));
            }
        }
    }

    //! SharedPool holds/manages the pool chunks
    ULMMemoryPool *SharedPool;

    //! MemoryBuckets holds/manages the allocator memory buckets
    ULMMemoryBuckets < NumberOfBuckets, MaxStackElements,
        Log2PoolChunkSize, ZeroAllocationBase > MemoryBuckets;

    //! lock for device
    Locks lock_m;

    //! initialization function.
    int Init(size_t PageSize,
             long *minPagesPerStack, long defaultMinPagesPerStack,
             long defaultNPagesPerStack, long *maxPagesPerStack,
             long *mxConsecReqFailures, char *lstlistDescription,
             long maxPagePoolSize, int *Log2BucketSize, bool Abort)
        {
            int returnValue = 0;

            // set failure mode when resource requests fail too often
            if (Abort)
                RequestFailedFunction = &ULMMallocSharedLocal::AbortFunction;
            else
                RequestFailedFunction = &ULMMallocSharedLocal::ReturnError;

            // set string describing the class
            size_t strLen = strlen(lstlistDescription) + 1;
            listDescription_m = (char *) ulm_malloc(strLen);
            if (!listDescription_m) {
                ulm_exit(("SharedMemoryFreeLists::Init Unable "
                          "to allocate memory for listDescription."
                          "size reqested %ld, errno %d\n", strlen, errno));
            }
            strncpy(listDescription_m, lstlistDescription, strLen);
            listDescription_m[strLen - 1] = '\0';

            if (ZeroAllocationBase) {
                MemoryBuckets.AllocationBase = 0;
            } else {
                // set upper limit on pool memory - actually mmap the pool
                // if necessary
                if (ownsSharedPool) {
                    SharedPool->SetPoolSize(maxPagePoolSize * PageSize);
                }
                // set AllocationBase - the pool base.  This is needed for the ULMFree
                // routine to figure out which chunk the memory was taken from, and
                // therefor which bucket to return the memory to.
                MemoryBuckets.AllocationBase = SharedPool->PoolBase;
            }

            // set pointer to array with information about the memory blocks.
            MemoryBuckets.PoolChunks = SharedPool->PoolChunks;

            // initialize the memory buckets
            returnValue = MemoryBuckets.Init(PageSize, minPagesPerStack,
                                             defaultMinPagesPerStack,
                                             defaultNPagesPerStack,
                                             maxPagesPerStack,
                                             mxConsecReqFailures,
                                             maxPagePoolSize, Log2BucketSize,
                                             lstlistDescription, Abort);

            // return value of 0 - all ok
            return returnValue;
        }

    /*!
     * This routine tries to grabs a chunk of memory from the
     * the memory pool.
     * \param bucketIndex the free list to modify
     * \param len the amount of memory added
     * \param errCode set by method if an error occurs 
     * \return NULL  - unable to add memory to list
     *         value - pointer to base of (contiguous) chunk allocated
     *
     * Note:: it is assumed that if this routine is called, list #bucketIndex
     *        is locked.
     */
    void *GetChunkOfMemory(int bucketIndex, size_t * len,
                           int *errCode, int stackNumber = 0) {
        void *ReturnValue = (void *) (-1L);


        // check to make sure that the amount to add to the stack does not 
        // exceed the amount allowed
        bool okToAdd = true;
        long long sizeToAdd = MemoryBuckets.PoolChunkSize;

        // check to see that we will not exceed the stack upper limit
        if (sizeToAdd +
            MemoryBuckets.buckets[bucketIndex].totalBytesAvailable_m >
            MemoryBuckets.buckets[bucketIndex].maxBytesPushedOnStack_m)
            okToAdd = false;

        // check to make sure that the stack is large enough to hold the 
        // new elements
        long maxStackEle =
            MemoryBuckets.buckets[bucketIndex].stack_m.MaxStkElems;
        long sizeAfterAdding =
            (sizeToAdd +
             MemoryBuckets.buckets[bucketIndex].totalBytesAvailable_m) /
            MemoryBuckets.buckets[bucketIndex].segmentSize_m;
        if (okToAdd && (sizeAfterAdding > maxStackEle))
            okToAdd = false;

        if (!okToAdd) {
            // increment failure count
            MemoryBuckets.buckets[bucketIndex].consecReqFail_m++;
            if (MemoryBuckets.buckets[bucketIndex].consecReqFail_m >=
                MemoryBuckets.buckets[bucketIndex].maxConsecReqFail_m) {
                *errCode = ULM_ERR_OUT_OF_RESOURCE;
                char *str1 = " Error:: Out of memory in pool for ";
                ssize_t len = sizeof(ssize_t);
                ssize_t totalLen =
                    len + strlen(str1) + strlen(listDescription_m) + 1;
                char *errorString = (char *) ulm_malloc(totalLen);
                sprintf(errorString, "%s%s\n", str1, listDescription_m);
                (this->*(this->RequestFailedFunction)) (errorString);
                ulm_err(("Error: %s\n", errorString));
                ulm_free(errorString);
            } else {
                *errCode = ULM_ERR_TEMP_OUT_OF_RESOURCE;
            }
            return ReturnValue;
        }                       // end check to see if can add more to stack

        // set len
        *len = sizeToAdd;

        // get chunk of memory
        ReturnValue =
            SharedPool->RequestChunk(bucketIndex, true, stackNumber);
        if (ReturnValue == (void *) -1L) {
            // increment failure count
            MemoryBuckets.buckets[bucketIndex].consecReqFail_m++;
            if (MemoryBuckets.buckets[bucketIndex].consecReqFail_m >=
                MemoryBuckets.buckets[bucketIndex].maxConsecReqFail_m) {
                *errCode = ULM_ERR_OUT_OF_RESOURCE;
                char *str1 = " Error:: Out of memory in pool for ";
                ssize_t len = sizeof(ssize_t);
                ssize_t totalLen =
                    len + strlen(str1) + strlen(listDescription_m) + 1;
                char *errorString = (char *) ulm_malloc(totalLen);
                sprintf(errorString, "%s%s\n", str1, listDescription_m);
                (this->*(this->RequestFailedFunction)) (errorString);
                ulm_err(("Error: %s\n", errorString));
                ulm_free(errorString);
            } else
                *errCode = ULM_ERR_TEMP_OUT_OF_RESOURCE;

            return ReturnValue;
        }
        // set consecutive failure count to 0 - if we fail, we don't get this far
        //   in the code.
        MemoryBuckets.buckets[bucketIndex].consecReqFail_m = 0;

        return ReturnValue;
    }

    /*!
     * This routine appends devides up a chunk of memory into small
     * frags, and appends them to the specified free pool stack.
     * bucketIndex - is the free stack to modify
     * chunkPtr - pointer to chunk of memory to add to the stack
     *
     * Note:: it is assumed that if this routine is called, stack #bucketIndex
     *        is locked.
     */

    void PushChunkOnStack(int bucketIndex, char *chunkPtr) {
        // push items onto stack 
        int eleSize = MemoryBuckets.buckets[bucketIndex].segmentSize_m;
        long nToPushAppendToFreeList =
            MemoryBuckets.PoolChunkSize / eleSize;
        char *tmpPtr = chunkPtr;

        // lock stack
        MemoryBuckets.buckets[bucketIndex].lock_m.lock();

        // push elements onto stack
        for (int ele = 0; ele < nToPushAppendToFreeList; ele++) {
            MemoryBuckets.buckets[bucketIndex].stack_m.push(tmpPtr);
            tmpPtr += eleSize;
        }

        // adjust memory counters
        MemoryBuckets.buckets[bucketIndex].totalBytesInBucket_m +=
            MemoryBuckets.PoolChunkSize;
        MemoryBuckets.buckets[bucketIndex].totalBytesAvailable_m +=
            MemoryBuckets.PoolChunkSize;

        // lock stack
        MemoryBuckets.buckets[bucketIndex].lock_m.unlock();

        return;
    }

private:
    //! list description
    char *listDescription_m;

    /*! flag indicates if object owns the raw memory pool pointed 
     * to by SharedPool */
    bool ownsSharedPool;

    /*! pointer to function called when too many consecutive requests for
     * memory have failed. */
    int (ULMMallocSharedLocal::*RequestFailedFunction) (char *ErrorString);

    //! functions to call when too many reqests have failed - abort function
    int AbortFunction(char *ErrorString) {
        ulm_exit((ErrorString));
        return 1;               // this line is never reached
    }

    //! return out of resource
    int ReturnError(char *ErrorString) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
}
;



#endif /* _ULMMALLOCSHAREDLOCAL */
