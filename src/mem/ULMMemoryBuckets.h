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

#ifndef _ULMMEMORYBUCKETS
#define _ULMMEMORYBUCKETS

#include <stdlib.h>
#include <assert.h>

#include "mem/ULMPool.h"
#include "mem/ULMStack.h"
#include "mem/Bucket.h"
#include "mem/ULMMallocMacros.h"
#include "ulm/ulm.h"
#include "internal/log.h"

template <int NumberOfBuckets, long MaxStackElements, int Log2PoolChunkSize, 
          bool ZeroAllocationBase>
class ULMMemoryBuckets
{
public:

    // --STATE--

    // number of buckets in use
    enum { NumBuckets = NumberOfBuckets };

    // array of bucket sizes
    Bucket < MaxStackElements > buckets[NumberOfBuckets];

    // bucket size
    enum { PoolChunkSize = 1 << Log2PoolChunkSize };

    // allocation pool base address
    void *AllocationBase;

    // pointer to PoolChunks array
    PoolChunks_t *PoolChunks;


    // --METHODS--

    // default consturctor
    ULMMemoryBuckets()
        {
        }

    // bucket initialization
    int InitialzeBuckets(int *Log2BucketSize, void *AllocBase,
                         void *PlChnks)
        {
            int RetVal = 0;

            // fill in bucket sizes
            for (int BucketIndex = 0; BucketIndex < NumBuckets; BucketIndex++) {
                buckets[BucketIndex].segmentSize_m =
                    1 << Log2BucketSize[BucketIndex];
                buckets[BucketIndex].nElements_m = 0;
            }

            // initialize statistics

            for (int i = 0; i < NumBuckets; i++) {
                // the TOTAL number of bytes in the bucket available for allocation
                buckets[i].totalBytesInBucket_m = 0;
                // the TOTAL number of bytes currently allocated from the bucket
                buckets[i].totalBytesAllocated_m = 0;
                // the TOTAL number of bytes available
                buckets[i].totalBytesAvailable_m = 0;
                // consecutive number of times a request for memory has failed
                buckets[i].consecReqFail_m = 0;
            }

            // set allocation base
            AllocationBase = AllocBase;

            // pointer to pool block control structures
            PoolChunks = (PoolChunks_t *) PlChnks;

            return RetVal;
        }

    // initialize the buckets 
    int Init(size_t PageSize,
             long *minPagesPerList, long defaultMinPagesPerList,
             long defaultNPagesPerList, long *maxPagesPerList,
             long *mxConsecReqFailures, long maxPagePoolSize,
             int *Log2BucketSize, char *listDescription, bool Abort)
        {

            // sanity check
            assert(maxPagePoolSize > 0);

            // return value 0 - ok
            //              1 - problem
            int returnValue = 0;

            // set failure mode when resource requests fail too often
            if (Abort)
                RequestFailedFunction = &ULMMemoryBuckets::AbortFunction;
            else
                RequestFailedFunction = &ULMMemoryBuckets::ReturnError;

            // determine minimum amount per bucket
            // loop over buckets
            long minPages = 0;
            for (int bucket = 0; bucket < NumBuckets; bucket++) {
                // if
                //   minimum == -1, set to defualt value
                if (minPagesPerList[bucket] == -1) {
                    buckets[bucket].minBytesPushedOnStack_m =
                        defaultMinPagesPerList * PageSize;
                    minPages += defaultMinPagesPerList;
                } else {
                    // else
                    //   set to specified value
                    buckets[bucket].minBytesPushedOnStack_m =
                        minPagesPerList[bucket] * PageSize;
                    minPages += minPagesPerList[bucket];
                }

            }                       // end bucket loop

            // if minimum allocation exceeds pool upper limit, 
            //   call error function
            if (minPages > maxPagePoolSize) {
                ulm_exit((-1,
                          "ULMMemoryBuckets::Init Too much memory requested "
                          "from pool, minpages %ld, maxPagePoolSize %ld\n",
                          minPages, maxPagePoolSize));
            }
            // determine maximum amount per bucket
            for (int bucket = 0; bucket < NumBuckets; bucket++) {
                // if
                //   maximum == -1, set to defualt value
                // use defalut upper limit
                if (maxPagesPerList[bucket] == -1) {
                    buckets[bucket].maxBytesPushedOnStack_m =
                        (maxPagePoolSize - minPages) * PageSize +
                        buckets[bucket].minBytesPushedOnStack_m;
                } else {
                    // else
                    //   set to specified value
                    buckets[bucket].maxBytesPushedOnStack_m =
                        maxPagesPerList[bucket] * PageSize;
                    // if max limit is above total amount availible in
                    //   pool, reset the upper limit.
                    if (maxPagesPerList[bucket] >
                        ((maxPagePoolSize - minPages) +
                         (ssize_t) (buckets[bucket].minBytesPushedOnStack_m /
                                    PageSize)))
                        buckets[bucket].maxBytesPushedOnStack_m =
                            (maxPagePoolSize - minPages) * PageSize +
                            buckets[bucket].minBytesPushedOnStack_m;
                }
            }                       // end bucket loop

            // fill in bucket sizes
            for (int BucketIndex = 0; BucketIndex < NumBuckets; BucketIndex++) {

                buckets[BucketIndex].segmentSize_m =
                    1 << Log2BucketSize[BucketIndex];
                // sanity check
                assert(PoolChunkSize >= buckets[BucketIndex].segmentSize_m);

                buckets[BucketIndex].nElements_m = 0;
                buckets[BucketIndex].totalBytesInBucket_m = 0;
                buckets[BucketIndex].totalBytesAllocated_m = 0;
                buckets[BucketIndex].totalBytesAvailable_m = 0;
                buckets[BucketIndex].maxConsecReqFail_m =
                    mxConsecReqFailures[BucketIndex];
                buckets[BucketIndex].consecReqFail_m = 0;
            }

            // return value of 0 - all ok
            return returnValue;
        }

    // allocate memory segment
    void *ULMMalloc(ssize_t size)
        {
            // returned pointer
            void *ReturnPtr = ((void *) -1L);

            // figure out bucket index
            int BucketIndex = -1;
            for (int i = 0; i < NumberOfBuckets; i++) {
                if (size <= buckets[i].segmentSize_m) {
                    BucketIndex = i;
                    break;
                }
            }

            assert(BucketIndex >= 0);

            // get element off stack
            ReturnPtr = buckets[BucketIndex].stack_m.pop();

            return ReturnPtr;
        }

    // free memory segment
    void ULMFree(void *Ptr)
        {

            // find bucket index

            // get offset relative to the base of the memory pool
            ptrdiff_t OffsetFromBase;
            if (!ZeroAllocationBase)
                OffsetFromBase = (char *) Ptr - (char *) AllocationBase;
            else
                OffsetFromBase = (ptrdiff_t) Ptr;

            // compute chunk index
            long ChunkIndex = OffsetFromBase >> Log2PoolChunkSize;

            // get bucket index
            int BucketIndex =
                (((PoolChunks_t *) PoolChunks) + ChunkIndex)->bucketindex;

            // push element onto stack
#ifdef NDEBUG
            buckets[BucketIndex].stack_m.push(Ptr);
#else
            int ReturnValue = buckets[BucketIndex].stack_m.push(Ptr);
            assert(ReturnValue == 0);
#endif
        }


    // get bucket index for given length of memory segment
    int GetBucketIndex(ssize_t size)
        {
            int BucketIndex = -1;
            for (int i = 0; i < NumberOfBuckets; i++) {
                if (size <= buckets[i].segmentSize_m) {
                    BucketIndex = i;
                    break;
                }
            }

            // sanity checks 
            assert(BucketIndex >= 0);
            assert(BucketIndex < NumberOfBuckets);

            // reuturn
            return BucketIndex;
        }


    // add a chunk to a memory segment - break it up into
    //   smaller segments, and push them onto the stack
    void AddChunk(void *Ptr, int BucketIndex)
        {
            // get chunk size
            long ChunkSize = 1 << Log2PoolChunkSize;

            // get number of segments to put on stack
            int SegSize = buckets[BucketIndex].segmentSize_m;
            long NumSegsToPush = ChunkSize / SegSize;

            char *StartingAddress = (char *) Ptr;
            buckets[BucketIndex].stack_m.Lock.lock();
            // loop over segments
            for (long Seg = 0; Seg < NumSegsToPush; Seg++) {
                // check and make sure that there is room on the stack
                // !!!! turn this into a debug only check - we now check before limits
                //   before trying to add to the stack
                if (buckets[BucketIndex].stack_m.TopOfStack ==
                    (buckets[BucketIndex].stack_m.MaxStkElems - 1)) {
                    ulm_warn(("Warning: ULMMalloc stack %d is full.\n"
                              "Segment number %ld top of stack %ld\n",
                              BucketIndex, Seg,
                              buckets[BucketIndex].stack_m.TopOfStack));
                    break;
                }
                // push segment onto the stack
                buckets[BucketIndex].stack_m.TopOfStack++;

                buckets[BucketIndex].stack_m.stackel
                    [buckets[BucketIndex].stack_m.TopOfStack] =
                    (void *) StartingAddress;

                // get base address of next segment
                StartingAddress += SegSize;
            }

            buckets[BucketIndex].stack_m.Lock.unlock();

        }

    // get chunk of memory - separate call to push this chunk
    //   unto the stack
    void *GetChunkOfMemory(int buckeIndex, size_t * len, int *errCode);

    // push chunk of memory onto specified stack
    void ppendToFreeList(int ListIndex, char *chunkPtr);

private:
    // pointer to function called when too many consecutive requests for
    //   memory have faile.
    int (ULMMemoryBuckets::*RequestFailedFunction) (char *ErrorString);

    // functions to call when too many reqests have failed.
    // abort function
    int AbortFunction(char *ErrorString) {
        ulm_exit((-1, ErrorString));
        // this line is never called
        return 1;
    }

    // return out of resource
    int ReturnError(char *ErrorString) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
}
;

#endif /* _ULMMEMORYBUCKETS */
