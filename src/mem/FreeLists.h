/*
 * This file is part of LA-MPI
 *
 * Copyright 2002 Los Alamos National Laboratory
 *
 * This software and ancillary information (herein called "LA-MPI") is
 * made available under the terms described here.  LA-MPI has been
 * approved for release with associated LA-CC Number LA-CC-02-41.
 * 
 * Unless otherwise indicated, LA-MPI has been authored by an employee
 * or employees of the University of California, operator of the Los
 * Alamos National Laboratory under Contract No.W-7405-ENG-36 with the
 * U.S. Department of Energy.  The U.S. Government has rights to use,
 * reproduce, and distribute LA-MPI. The public may copy, distribute,
 * prepare derivative works and publicly display LA-MPI without
 * charge, provided that this Notice and any statement of authorship
 * are reproduced on all copies.  Neither the Government nor the
 * University makes any warranty, express or implied, or assumes any
 * liability or responsibility for the use of LA-MPI.
 * 
 * If LA-MPI is modified to produce derivative works, such modified
 * LA-MPI should be clearly marked, so as not to confuse it with the
 * version available from LANL.
 * 
 * LA-MPI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * LA-MPI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef _FREELISTS
#define _FREELISTS

#include <new>

#include "util/Links.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/options.h"
#include "internal/types.h"
#include "mem/MemoryPool.h"
#include "mem/FixedSharedMemPool.h"
#include "mem/MemorySegments.h"
#include "ulm/ulm.h"

#if defined(NUMA) && defined(__mips)
#include "os/IRIX/SN0/acquire.h"
#endif

/* Externs */
extern FixedSharedMemPool PerProcSharedMemoryPools;

/*
 * This template is used to manage shared memory free lists.  
 *
 * ContainerType - Type of linked list used to hold the available 
 * descriptors in the FreeLists::freeLists_m array of linked
 * lists.
 *
 * ElementType - Type of the descriptors stored in the 
 * FreeLists::freeLists_m array.
 *
 * MemProt - Type of memory protection to use on the memory 
 * managed by the FreeLists object.
 *
 * MemFlags - Flags passed to mmap.
 *
 * SharedMemFlag - Flag indicating that the memory managed by 
 * the FreeLists is shared memory.
 */

template < class ContainerType, class ElementType,
           int MemProt, int MemFlags, int SharedMemFlag >
class FreeLists {
public:
    // --STATE--
    // string describing the pool
    char *listDescription;

    // number of free lists
    int nLists_m;

    // Number of elements per chunk
    int nElementsPerChunk_m;

    // element size (in bytes)
    size_t eleSize_m;

    // Memory pool - source of memory for free lists
    MemoryPool < MemProt, MemFlags, SharedMemFlag > *memoryPool_m;

    // flag on whether to call ulm_free(memoryPool_m) at destruction
    bool freeMemPool_m;

    // --METHODS--
    // d'tor
    ~FreeLists()
        {
            if (affinity_m) {
                delete[]affinity_m;
                affinity_m = 0;
            }

            if (memoryPool_m) {
                memoryPool_m->MemoryPool < MemProt, MemFlags,
                    SharedMemFlag >::~MemoryPool();
                if (freeMemPool_m && (MemFlags != SharedMemFlag)) {
                    ulm_free(memoryPool_m);
                    memoryPool_m = 0;
                }
            }

            if (OPT_MEMPROFILE) {
                // print out log info
                mem_log();

                // cleanup
                if (elementsOut) {
                    ulm_free(elementsOut);
                    elementsOut = 0;
                }
                if (elementsMax) {
                    ulm_free(elementsMax);
                    elementsMax = 0;
                }
                if (elementsSum) {
                    ulm_free(elementsSum);
                    elementsSum = 0;
                }
                if (numEvents) {
                    ulm_free(numEvents);
                    numEvents = 0;
                }
                if (chunksRequested) {
                    ulm_free(chunksRequested);
                    chunksRequested = 0;
                }
                if (chunksReturned) {
                    ulm_free(chunksReturned);
                    chunksReturned = 0;
                }
            }

            if (listDescription) {
                ulm_free(listDescription);
                listDescription = 0;
            }

        }

    // functions to call when too many reqests have failed.
    // abort function
    int AbortFunction(char *ErrorString)
        {
            ulm_exit((-1, ErrorString));
            // this line is never called
            return 1;
        }

    // return out of resource
    int ReturnError(char *ErrorString)
        {
            return ULM_ERR_OUT_OF_RESOURCE;
        }

    // initialization
    int Init(int Nlsts, long nPagesPerList,
             ssize_t PoolChunkSize, size_t PageSize,
             ssize_t ElementSize, long minPagesPerList,
             long maxPagesPerList, long mxConsecReqFailures,
             char *lstlistDescription, int retryForMoreResources,
             affinity_t *affinity, bool enforceMemAffinity,
             MemoryPool < MemProt, MemFlags,
             SharedMemFlag > *inputPool, bool Abort =
             true, int threshToGrowList = 0, bool freeMemPool
             = true)
        {

            int errorCode = ULM_SUCCESS;

            // sanity checks
            assert(PoolChunkSize >= ElementSize);

            // set failure mode when resource requests fail too often
            if (Abort) {
                RequestFailedFunction = &FreeLists::AbortFunction;
            } else {
                RequestFailedFunction = &FreeLists::ReturnError;
            }

            // set string describing the class
            int strLen = strlen(lstlistDescription) + 1;
            listDescription = (char *) ulm_malloc(strLen);
            if (!listDescription) {
                ulm_exit((-1, "ProcessPrivateFreeLists::Init Unable to "
                          "allocate memory for listDescription.  Tried "
                          " to allocate %ld bytes, errno %d \n",
                          strlen, errno));
            }
            strncpy(listDescription, lstlistDescription, strLen);
            listDescription[strLen - 1] = '\0';

            // Initialize number of contexts
            nLists_m = Nlsts;

            // initialize data for memory profiling
            if (OPT_MEMPROFILE) {
                elementsOut = (int *) ulm_malloc(nLists_m * sizeof(int));
                elementsMax = (int *) ulm_malloc(nLists_m * sizeof(int));
                elementsSum = (int *) ulm_malloc(nLists_m * sizeof(int));
                numEvents = (int *) ulm_malloc(nLists_m * sizeof(int));
                chunksRequested = (int *) ulm_malloc(nLists_m * sizeof(int));
                chunksReturned = (int *) ulm_malloc(nLists_m * sizeof(int));
                if (!elementsOut || !elementsMax || !elementsSum
                    || !numEvents || !chunksRequested || !chunksReturned) {
                    ulm_exit((-1, "Error malloc'ing arrays for memory"
                              " profiling \n"));
                }
                for (int i = 0; i < nLists_m; i++) {
                    elementsOut[i] = 0;
                    elementsMax[i] = 0;
                    elementsSum[i] = 0;
                    numEvents[i] = 0;
                    chunksRequested[i] = 0;
                    chunksReturned[i] = 0;
                }
            } else {
                elementsOut = 0;
                elementsMax = 0;
                elementsSum = 0;
                numEvents = 0;
                chunksRequested = 0;
                chunksReturned = 0;
            }

            // set threshold for adding more element to a free list
            thresholdToGrowList = threshToGrowList;

            // set element size
            eleSize_m = ElementSize;

            // set destructor free memory pool flag
            freeMemPool_m = freeMemPool;

            // set pool to be used
            if (inputPool) {
                // use pool passed in
                memoryPool_m = inputPool;
            } else {
                // instantiate pool
                ssize_t maxMemoryInPool = maxPagesPerList * PageSize;

                // initialize memory pool
                errorCode = memPoolInit(Nlsts, nPagesPerList, PoolChunkSize,
                                        PageSize, minPagesPerList,
                                        minPagesPerList, nPagesPerList,
                                        maxPagesPerList, maxMemoryInPool);
                if (errorCode != ULM_SUCCESS) {
                    return errorCode;
                }
            }

            // reset pool chunk size
            PoolChunkSize = memoryPool_m->ChunkSize;

            // Number of elements per chunk
            nElementsPerChunk_m = PoolChunkSize / ElementSize;

            ssize_t initialMemoryPerList = minPagesPerList * PageSize;

            // adjust initialMemoryPerList to increments of PoolChunkSize
            if (initialMemoryPerList < PoolChunkSize) {
                minPagesPerList = (((PoolChunkSize - 1) / PageSize) + 1);
                initialMemoryPerList = minPagesPerList * PageSize;
            }
            // determine upper limit on number of pages in a given list
            if (maxPagesPerList != -1 && maxPagesPerList < minPagesPerList)
                maxPagesPerList = minPagesPerList;

            long maxMemoryPerList;
            if (maxPagesPerList == -1)
                maxMemoryPerList = -1;
            else
                maxMemoryPerList = maxPagesPerList * PageSize;

            // initialize empty lists of available descriptors
            freeLists_m = (ListOfMemorySegments_t < ContainerType > **)
                ulm_malloc(sizeof(ListOfMemorySegments_t < ContainerType > *) *
                           nLists_m);
            if (!freeLists_m) {
                ulm_exit((-1, "ProcessPrivateFreeLists::Init Unable to "
                          "allocate memory for freeList_m.  Tried to "
                          "allocate %ld bytes, errno %d \n",
                          sizeof(ListOfMemorySegments_t < ContainerType > *)
                          * nLists_m, errno));
            }
            // run constructors
            for (int list = 0; list < nLists_m; list++) {
                if (MemFlags == SharedMemFlag) {
                    // process shared memory allocation
                    freeLists_m[list] =
                        (ListOfMemorySegments_t < ContainerType > *)
                        PerProcSharedMemoryPools.
                        getMemorySegment(sizeof
                                         (ListOfMemorySegments_t <
                                          ContainerType >),
                                         CACHE_ALIGNMENT, list);
                } else {
                    // process private memory allocation
                    freeLists_m[list] =
                        (ListOfMemorySegments_t < ContainerType > *)
                        ulm_malloc(sizeof
                                   (ListOfMemorySegments_t < ContainerType >));
                }
                if (!freeLists_m[list]) {
                    ulm_exit((-1, "FreeLists::Init Unable to allocate "
                              "memory for freeLists_m[]. size reqested %ld "
                              "errno %d \n",
                              sizeof(ListOfMemorySegments_t < ContainerType >),
                              errno));
                }
                new(freeLists_m[list]) ListOfMemorySegments_t <
                    ContainerType >;

                freeLists_m[list]->minBytesPushedOnFreeList_m =
                    initialMemoryPerList;
                freeLists_m[list]->maxBytesPushedOnFreeList_m =
                    maxMemoryPerList;
                freeLists_m[list]->bytesPushedOnFreeList_m = 0;
                freeLists_m[list]->maxConsecReqFail_m = mxConsecReqFailures;
                freeLists_m[list]->consecReqFail_m = 0;
            }                       // end list loop

            retryForMoreResources_m = retryForMoreResources;
            enforceMemAffinity_m = enforceMemAffinity;
            if (enforceMemAffinity_m) {
                affinity_m = new affinity_t[nLists_m];
                if (!affinity_m) {
                    ulm_exit((-1,
                              "Unable to allocate affinity_m array.\n"));
                }
                // copy policies in
                for (int pool = 0; pool < nLists_m; pool++) {
                    affinity_m[pool] = affinity[pool];
                }
            }
            // initialize locks for memory pool and individual list and link locks
            for (int i = 0; i < nLists_m; i++) {

                // gain exclusive use of list
                if (freeLists_m[i]->lock_m.trylock() == 1) {

                    while (freeLists_m[i]->bytesPushedOnFreeList_m
                           < freeLists_m[i]->minBytesPushedOnFreeList_m) {
                        if (createMoreElements(i) != ULM_SUCCESS) {
                            ulm_exit((-1, "Error setting up initial private "
                                      "free list for %s.\n", listDescription));
                        }
                    }               // end while loop

                    freeLists_m[i]->lock_m.unlock();

                } else {
                    // only 1 process should be initializing the list
                    ulm_exit((-1, "Error setting up initial private free "
                              "list %d for %s. \n", i, listDescription));
                }
            }                       // end freeList loop

            return errorCode;
        }

    /*
     * Add chunk of memory for the specified free list
     *   ListIndex - is the free list to modify
     *   len - on output is the amount of memory added
     *   return value
     *     NULL - unable to add memory to list
     *     -1   - list is being modified by someone else
     *     some value - pointer to base of (contiguous) chunk
     *                  allocated
     *  Note:: it is assumed that if this routine is called, list #ListIndex
     *         is locked.
     */
    void *GetChunkOfMemory(int ListIndex, size_t * len, int *errCode)
        {
            void *ReturnValue = (void *) (-1L);
            // check to make sure that the amount to add to the list does not 
            // exceed the amount allowed
            long long sizeToAdd = memoryPool_m->ChunkSize;

            if (OPT_MEMPROFILE) {
                chunksRequested[ListIndex]++;
            }

            if (ListIndex >= nLists_m) {
                ulm_err(("Array out of bounds!! \n"));
            }
            if (!(freeLists_m[ListIndex]->maxBytesPushedOnFreeList_m == -1)) {
                if (sizeToAdd +
                    freeLists_m[ListIndex]->bytesPushedOnFreeList_m >
                    freeLists_m[ListIndex]->maxBytesPushedOnFreeList_m) {
                    freeLists_m[ListIndex]->consecReqFail_m++;
                    if (freeLists_m[ListIndex]->consecReqFail_m >=
                        freeLists_m[ListIndex]->maxConsecReqFail_m) {
                        *errCode = ULM_ERR_OUT_OF_RESOURCE;
                        ulm_err(("GetChunkOfMemory: Out of memory in "
                                 "pool for %s \n", listDescription));
                        return ReturnValue;
                    } else
                        *errCode = ULM_ERR_TEMP_OUT_OF_RESOURCE;
                    freeLists_m[ListIndex]->lock_m.unlock();
                    return ReturnValue;
                }
            }
            // set len
            *len = sizeToAdd;


            // get chunk of memory
            ReturnValue = memoryPool_m->RequestChunk(ListIndex);
            if (ReturnValue == (void *) -1L) {
                // increment failure count
                freeLists_m[ListIndex]->consecReqFail_m++;
                if (freeLists_m[ListIndex]->consecReqFail_m
                    >= freeLists_m[ListIndex]->maxConsecReqFail_m) {
                    *errCode = ULM_ERR_OUT_OF_RESOURCE;
                    ulm_err(("GetChunkOfMemory: Out of memory in "
                             "pool for %s \n", listDescription));
                    return ReturnValue;
                } else
                    *errCode = ULM_ERR_TEMP_OUT_OF_RESOURCE;
                // unlock device
                freeLists_m[ListIndex]->lock_m.unlock();
                return ReturnValue;
            }
            // set consecutive failure count to 0 - if we fail, we don't get 
            // this far in the code.
            freeLists_m[ListIndex]->consecReqFail_m = 0;

            if (OPT_MEMPROFILE) {
                chunksReturned[ListIndex]++;
            }

            return ReturnValue;
        }

    /*
     * This routine divides up a chunk of memory into small
     * frags, and appends them to the specified free pool list.
     * ListIndex - is the free list to modify
     * chunkPtr - pointer to chunk of memory to add to the list
     *
     * Note:: it is assumed that if this routine is called, list #ListIndex
     *        is not locked.
     */
    void AppendToFreeList(int ListIndex, char *chunkPtr)
        {

            // push items onto list 
            freeLists_m[ListIndex]->freeList_m.AddChunk
                ((Links_t *) chunkPtr, nElementsPerChunk_m, eleSize_m);

            // adjust memory counters
            freeLists_m[ListIndex]->bytesPushedOnFreeList_m +=
                memoryPool_m->ChunkSize;

            return;
        }

    //  Append to free list - list is assumed to be locked
    void AppendToFreeListNoLock(int ListIndex, char *chunkPtr)
        {
            // push items onto list 
            freeLists_m[ListIndex]->freeList_m.AddChunkNoLock
                ((Links_t *) chunkPtr, nElementsPerChunk_m, eleSize_m);

            // adjust memory counters
            freeLists_m[ListIndex]->bytesPushedOnFreeList_m +=
                memoryPool_m->ChunkSize;

            return;
        }

    // Allocate and construct new elements (using placement new).
    // Append the new elements to the free list.
    int createMoreElements(int poolIndex)
        {
            size_t lenAdded;
            int errorCode = ULM_SUCCESS;

            void *basePtr = GetChunkOfMemory(poolIndex, &lenAdded, &errorCode);

            if (basePtr == (void *) (-1L)) {
#ifdef _DEBUGQUEUES
                fprintf(stderr,
                        " Unable to add new elements for free list %s\n",
                        listDescription);
                fflush(stderr);
#endif
                freeLists_m[poolIndex]->lock_m.unlock();
                return errorCode;
            }                       // end !basePtr

            // attach memory affinity
            if (enforceMemAffinity_m) {
                if (!setAffinity(basePtr, lenAdded,
                                  affinity_m[poolIndex])) {
                    errorCode = ULM_ERROR;
#ifdef _DEBUGQUEUES
                    fprintf(stderr,
                            "Unable to set memory policy :: poolIndex %d\n",
                            poolIndex);
                    return errorCode;
#endif                          /* _DEBUGQUEUES */
                }
            }
            // Construct new descriptors using placement new
            char *currentLoc = (char *) basePtr;
            for (int desc = 0; desc < nElementsPerChunk_m; desc++) {
                new(currentLoc) ElementType(poolIndex);
                currentLoc += eleSize_m;
            }

            // push chunk of memory onto the list
            AppendToFreeListNoLock(poolIndex, (char *) basePtr);

            return errorCode;
        }                           // end createMoreElements

    // Get an element from the specified element pool.  If no elements are
    // available, construct a pool of new ones to use, and return a valid
    // element, if possible.  Otherwise (ULM_ERR_OUT_OF_RESOURCE), return a
    // NULL pointer.
    inline ElementType *getElement(int listIndex, int &error)
        {
            volatile Links_t *elem = (ElementType *) (0);
            elem = RequestElement(listIndex);

            if (elem) {
                error = ULM_SUCCESS;
            } else if (freeLists_m[listIndex]->consecReqFail_m
                       < thresholdToGrowList) {
                error = ULM_ERR_TEMP_OUT_OF_RESOURCE;
            } else {
            RETRYTAG:
                error = ULM_ERR_TEMP_OUT_OF_RESOURCE;
                if (freeLists_m[listIndex]->freeList_m.Lock.trylock() == 1) {

                    error = createMoreElements(listIndex);
                    if ((error == ULM_ERR_OUT_OF_RESOURCE) ||
                        (error == ULM_ERR_FATAL)) {
                        freeLists_m[listIndex]->freeList_m.Lock.unlock();
                        return (ElementType *) (0);
                    }
                    // get element if managed to add resources to the list
                    if (error == ULM_SUCCESS) {
                        elem = RequestElementNoLock(listIndex);
                    }
                    freeLists_m[listIndex]->freeList_m.Lock.unlock();
                }
                // retry if requested to do so
                if ((!elem) && (retryForMoreResources_m)) {
                    goto RETRYTAG;
                }
            }
            if (OPT_MEMPROFILE) {
                elementsOut[listIndex]++;
                elementsSum[listIndex] += elementsOut[listIndex];
                numEvents[listIndex]++;
                if (elementsMax[listIndex] < elementsOut[listIndex]) {
                    elementsMax[listIndex] = elementsOut[listIndex];
                }
            }

            return (ElementType *) elem;
        }

    // lock-less version - no contention assumed
    // Get an element from the specified element pool.  If no elements are
    // available, construct a pool of new ones to use, and return a valid
    // element, if possible.  Otherwise (ULM_ERR_OUT_OF_RESOURCE), return a
    // NULL pointer.
    inline ElementType *getElementNoLock(int listIndex, int &error)
        {
            volatile Links_t *elem = (ElementType *) (0);
            elem = RequestElementNoLock(listIndex);

            if (elem) {
                error = ULM_SUCCESS;
            } else if (freeLists_m[listIndex]->consecReqFail_m
                       < thresholdToGrowList) {
                error = ULM_ERR_TEMP_OUT_OF_RESOURCE;
            } else {
            RETRYTAG:
                error = ULM_ERR_TEMP_OUT_OF_RESOURCE;
                if (freeLists_m[listIndex]->freeList_m.Lock.trylock() == 1) {

                    error = createMoreElements(listIndex);
                    if ((error == ULM_ERR_OUT_OF_RESOURCE)
                        || (error == ULM_ERR_FATAL)) {
                        freeLists_m[listIndex]->freeList_m.Lock.unlock();
                        return (ElementType *) (0);
                    }
                    // get element if managed to add resources to the list
                    if (error == ULM_SUCCESS) {
                        elem = RequestElementNoLock(listIndex);
                    }
                    freeLists_m[listIndex]->freeList_m.Lock.unlock();
                }
                // retry if requested to do so
                if ((!elem) && (retryForMoreResources_m)) {
                    goto RETRYTAG;
                }
            }
            if (OPT_MEMPROFILE) {
                elementsOut[listIndex]++;
                elementsSum[listIndex] += elementsOut[listIndex];
                numEvents[listIndex]++;
                if (elementsMax[listIndex] < elementsOut[listIndex]) {
                    elementsMax[listIndex] = elementsOut[listIndex];
                }
            }

            return (ElementType *) elem;
        }

    // return an element to a specified element pool with locking
    inline int returnElement(Links_t * e, int ListIndex = 0)
        {
            freeLists_m[ListIndex]->freeList_m.Append(e);

            if (OPT_MEMPROFILE) {
                elementsOut[ListIndex]--;
            }

            return ULM_SUCCESS;
        }

    // return an element to a specified element pool without locking
    inline int returnElementNoLock(Links_t * e, int ListIndex = 0)
        {
            mb();
            freeLists_m[ListIndex]->freeList_m.AppendNoLock(e);
            mb();

            if (OPT_MEMPROFILE) {
                elementsOut[ListIndex]--;
            }

            return ULM_SUCCESS;
        }

protected:
    // pointer to function called when too many consecutive requests for
    // memory have failed.
    int (FreeLists::*RequestFailedFunction) (char *ErrorString);

private:
    // list of available descriptors - one per recieving context, the list
    // of pointer to the MemorySegments_t resides in private memory, since
    // the pointers are allocated before the fork and never modified.
    ListOfMemorySegments_t < ContainerType > **freeLists_m;
    // if unable to get more resource, try again
    int retryForMoreResources_m;
    // indicates if memory affinity is to be enforced
    bool enforceMemAffinity_m;
    // array of memory policies - one per pool
    affinity_t *affinity_m;
    // at or above this value more elements are added to the list, 
    // if the list is empty
    int thresholdToGrowList;
    // number of elements  currently in use
    int *elementsOut;
    // maximum number of elements out
    int *elementsMax;
    // sum of elements used - for computing average
    int *elementsSum;
    // number of in/out events for elements - for computing aveerage
    int *numEvents;
    // number of memory chunks requested to add to freelist
    int *chunksRequested;
    // number of chunks actually added to freelist
    int *chunksReturned;

#include "os/numa.h"

    // request an element from a specified element pool
    inline volatile Links_t *RequestElement(int ListIndex = 0)
        {
            volatile Links_t *returnPtr =
                freeLists_m[ListIndex]->freeList_m.GetLastElement();
            if (returnPtr)
                freeLists_m[ListIndex]->consecReqFail_m = 0;
            return returnPtr;
        }

    // request an element from a specified element pool
    inline volatile Links_t *RequestElementNoLock(int ListIndex = 0)
        {
            volatile Links_t *returnPtr =
                freeLists_m[ListIndex]->freeList_m.GetLastElementNoLock();
            if (returnPtr)
                freeLists_m[ListIndex]->consecReqFail_m = 0;
            return returnPtr;
        }


    //! initialize memory pool
    int memPoolInit(int Nlsts, long nPagesPerList, ssize_t PoolChunkSize,
                    size_t PageSize, long minPagesPerList,
                    long defaultMinPagesPerList, long defaultNPagesPerList,
                    long maxPagesPerList, ssize_t maxMemoryInPool)
        {
            int errorCode = ULM_SUCCESS;

            // set chunksize - multiple of page size
            PoolChunkSize =
                ((((PoolChunkSize - 1) / PageSize) + 1) * PageSize);

            // determine number how much memory to allocate
            long totPagesToAllocate;
            if (nPagesPerList == -1) {
                // minimum size is  defaultNPagesPerList*number of local procs
                totPagesToAllocate = defaultNPagesPerList * Nlsts;
            } else {
                totPagesToAllocate = nPagesPerList * Nlsts;
            }

            ssize_t memoryInPool = totPagesToAllocate * PageSize;

            // Initialize memory pool
            size_t lenToAlloc;
            if (MemFlags == SharedMemFlag) {
                // shared memory allocation
                lenToAlloc =
                    sizeof(MemoryPool < MemProt, MemFlags, SharedMemFlag >);
                memoryPool_m =
                    (MemoryPool < MemProt, MemFlags, SharedMemFlag > *)
                    SharedMemoryPools.getMemorySegment(lenToAlloc,
                                                       CACHE_ALIGNMENT);
            } else {
                // process private memory allocation
                lenToAlloc =
                    sizeof(MemoryPool < MemProt, MemFlags, SharedMemFlag >);
                memoryPool_m =
                    (MemoryPool < MemProt, MemFlags, SharedMemFlag > *)
                    ulm_malloc(lenToAlloc);
            }
            if (!memoryPool_m) {
                ulm_exit((-1, "FreeLists::Init Unable to allocate memory "
                          "for memoryPool_m. size reqested %ld, "
                          "errno %d ", lenToAlloc, errno));
            }
            // run constructor
            new(memoryPool_m) MemoryPool < MemProt, MemFlags, SharedMemFlag >;

            errorCode = memoryPool_m->Init(memoryInPool, maxMemoryInPool,
                                           PoolChunkSize, PageSize);
            if (errorCode != ULM_SUCCESS) {
                return errorCode;
            }
            //! return
            return errorCode;
        }

    void mem_log()
        {
            FILE *fd;
            char fn[256];
            pid_t p;
            int i;

            p = getpid();
            sprintf(fn, "lampi_mem.log.%d", p);
            fd = fopen(fn, "a");
            if (fd) {
                for (i = 0; i < nLists_m; i++) {
                    fprintf(fd, "%s : %p : %d : %i : %d : %d : %d :"
                            " %d : %d \n", listDescription, this, p,
                            i, elementsMax[i], elementsSum[i],
                            numEvents[i], chunksRequested[i],
                            chunksReturned[i]);
                    fflush(fd);
                }
                fclose(fd);
            } else {
                fprintf(stderr, "Unable to append to lampi_mem.log");
                fflush(stderr);
            }
        }
};

#endif                          /* !_FREELISTS */
