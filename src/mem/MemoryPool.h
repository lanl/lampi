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


#ifndef _MEMORYPOOL
#define _MEMORYPOOL

#include "internal/mmap_params.h"
#include "internal/log.h"
#include "internal/new.h"
#include "util/Lock.h"
#include "ulm/ulm.h"
#include "mem/ULMMallocMacros.h"
#include "mem/ZeroAlloc.h"

#ifdef ENABLE_GM
#include <gm.h>
#endif


// Class used to manage a memory pool that is anonymously mmap-ed 
// so that it can be both process private or process shared 

template <int MemProt, int MemFlags, int SharedMemFlag>
class MemoryPool_t
{
    class ChunkDesc_t {
    public:
        unsigned short flags;   // usage flags
        int BucketIndex;        // which bucket does this belong to
        void *BasePtr;          // base pointer
    };

public:

    // --STATE--

    size_t ChunkSize;           // size of each chunk allocation - in bytes
    long NPoolChunks;           // number of chunks
    long maxNPoolChunks;        // maximum number of pool chunks
    Locks Lock;                 // lock
    size_t PageSize;            // page size
    ChunkDesc_t *ChunkDesc;     // array of information about each chunk
    long NextAvailChunk;        // next available chunk
#ifdef ENABLE_GM
    struct gm_port *gmPort;     // for mlocking Myrinet memory buffers
#endif

    // --METHODS--

    MemoryPool_t()
        {
#ifdef ENABLE_GM
            gmPort = 0;
#endif
        }

    ~MemoryPool_t()
        {
            if (MemFlags != SharedMemFlag && ChunkDesc) {
                ulm_delete(ChunkDesc);
                ChunkDesc = 0;
            }
        }

    // request chunk of memory
    void *RequestChunk(int poolIndex)
        {
            void *ReturnPtr = (void *) -1L;

            // grab lock on pool
            Lock.lock();

            // Have we used all the allocated memory ?
            if (NextAvailChunk == NPoolChunks) {

                // can we increase the pool size ?  We currently won't grow a shared
                //  memory region
                if ((MemFlags == SharedMemFlag) ||
                    ((maxNPoolChunks > 0)
                     && (maxNPoolChunks == maxNPoolChunks))) {
                    Lock.unlock();
                    return ReturnPtr;
                }
                // allocate larger array of chunk descriptors
                ChunkDesc_t *TmpDesc =
                    ulm_new(ChunkDesc_t, NPoolChunks + 1);
                // copy old array into new array
                for (int desc = 0; desc < NPoolChunks; desc++) {
                    TmpDesc[desc] = ChunkDesc[desc];
                }
                // free old array
                ulm_delete(ChunkDesc);
                // set old array pointer to point to new array
                ChunkDesc = TmpDesc;

                // allocate new memory chunk
                ChunkDesc[NPoolChunks].BasePtr =
                    ZeroAlloc(ChunkSize, MMAP_PRIVATE_PROT,
                              MMAP_PRIVATE_FLAGS);
                if (!ChunkDesc[NPoolChunks].BasePtr) {
                    ulm_err(("Error: Out of memory (ZeroAlloc)\n"));
                    Lock.unlock();
                    return ReturnPtr;
                }
#ifdef ENABLE_GM
                if (gmPort) {
                    gm_status_t returnValue =
                        gm_register_memory(gmPort,
                                           ChunkDesc[NPoolChunks].BasePtr,
                                           ChunkSize);
                    if (returnValue != GM_SUCCESS) {
                        ulm_err(("Error: Unable to register memory for GM (returnValue = %d)\n",
                                 (int) returnValue));
                        return ReturnPtr;
                    }
                }
#endif

                // reset pool chunk counter
                NPoolChunks++;
            }
            // grab chunk
            ReturnPtr = ChunkDesc[NextAvailChunk].BasePtr;
            ChunkDesc[NextAvailChunk].flags = ALLOCELEMENT_FLAG_INUSE;
            ChunkDesc[NextAvailChunk].BucketIndex = poolIndex;

            // find next available chunk
            bool freeChunkFound = false;
            int NextChunkToUse = NextAvailChunk + 1;
            while (NextChunkToUse < NPoolChunks) {

                if (ChunkDesc[NextChunkToUse].flags ==
                    ALLOCELEMENT_FLAG_AVAILABLE) {
                    NextAvailChunk = NextChunkToUse;
                    freeChunkFound = true;
                    break;
                }

                NextChunkToUse++;
            }
            // if no chunks available set next chunk past end of list so that next
            // time around more memory will be allocated
            if (!freeChunkFound) {
                NextAvailChunk = NPoolChunks;
            }
            // unlock pool
            Lock.unlock();

            return ReturnPtr;
        }

    // initialize memory pool
    // chunkSize is passed in as a page multiple.
    int Init(unsigned long long PoolSize, long long maxLen,
             long long PoolChunkSize, size_t PgSize)
        {
            // set page size
            PageSize = PgSize;
            if (((PageSize / getpagesize()) * getpagesize()) != PageSize) {
                return ULM_ERR_BAD_PARAM;
            }

            // set chunksize - multiple of page size
            ChunkSize = ((((PoolChunkSize - 1) / PageSize) + 1) * PageSize);
            if (ChunkSize == 0) {
                ulm_err(("Error: Chunksize == 0\n"));
                return ULM_ERR_BAD_PARAM;
            }
            // set upper limit on pool
            if (maxLen < 0) {
                // no upper limit on size
                maxNPoolChunks = -1;
            } else {
                maxNPoolChunks = ((maxLen - 1) / PageSize) + 1;
                if (maxNPoolChunks == 0) {
                    ulm_err(("Error: maxNPoolChunks == 0\n"));
                    return ULM_ERR_BAD_PARAM;
                }
            }

            // round up pool size to multiple of page size
            PoolSize = ((((PoolSize - 1) / ChunkSize) + 1) * ChunkSize);
            if (PoolSize == 0) {
                ulm_err(("Error: PoolSize == 0\n"));
                return ULM_ERR_BAD_PARAM;
            }

            if (PoolSize < ChunkSize) {
                ulm_err(("Error: PoolSize < ChunkSize\n"));
                return ULM_ERR_BAD_PARAM;
            }

            char *TmpPtr = 0;
            ssize_t WorkingSize = PoolSize;
            void *PoolBase = 0;
            while (!TmpPtr && WorkingSize) {
                // add red-zone pages
                ssize_t SizeToAllocate = WorkingSize + 2 * PageSize;

                // allocate memory
                TmpPtr = (char *) ZeroAlloc(SizeToAllocate, MemProt, MemFlags);

                if (TmpPtr == 0)
                    WorkingSize /= 2;

                // set base pointer
                if (TmpPtr) {
#ifdef ENABLE_GM
                    if (gmPort) {
                        gm_status_t returnValue =
                            gm_register_memory(gmPort,
                                               TmpPtr + PageSize,
                                               WorkingSize);
                        if (returnValue != GM_SUCCESS) {
                            ulm_err(("Error: Unable to register memory for GM (returnValue = %d)\n",
                                     (int) returnValue));
                            return ULM_ERROR;
                        }
                    }
#endif
                    PoolBase = (void *) (TmpPtr + PageSize);
                }

            }
            // reset pool size
            PoolSize = WorkingSize;
            NPoolChunks = ((PoolSize - 1) / ChunkSize) + 1;
            if ((NPoolChunks > maxNPoolChunks) && (maxNPoolChunks > 0)) {
                ulm_err(("Error: NPoolChunks (%ld) > maxNPoolChunks (%ld)\n",
                         NPoolChunks, maxNPoolChunks));
                return ULM_ERR_BAD_PARAM;
            }
            // change memory protection for red zones
            int retval = mprotect(TmpPtr, PageSize, PROT_NONE);
            if (retval != 0) {
                ulm_exit((-1, "Error in red zone 1 mprotect\n"));
            }
            // end red zone
            retval =
                mprotect(TmpPtr + PageSize + WorkingSize, PageSize, PROT_NONE);
            if (retval != 0) {
                ulm_exit((-1, "Error in red zone 2 mprotect\n"));
            }
            // initialize chunk descriptors
            if (MemFlags == SharedMemFlag) {
                // shared Memory allocation
                size_t lenToAlloc = sizeof(ChunkDesc_t) * NPoolChunks;
                ChunkDesc =
                    (ChunkDesc_t *) ZeroAlloc(lenToAlloc, MemProt, MemFlags);
                if (!ChunkDesc) {
                    ulm_err(("Error: Out of memory (ZeroAlloc)\n"));
                    return ULM_ERROR;
                }
            } else {
                // process private Memory allocation
                ChunkDesc = ulm_new(ChunkDesc_t, NPoolChunks);
                if (!ChunkDesc) {
                    ulm_err(("Error: Out of memory\n"));
                    fflush(stderr);
                    return ULM_ERROR;
                }
            }

            TmpPtr = (char *) PoolBase;
            for (int chunk = 0; chunk < NPoolChunks; chunk++) {
                ChunkDesc[chunk].flags = ALLOCELEMENT_FLAG_AVAILABLE;
                ChunkDesc[chunk].BucketIndex = -1;
                ChunkDesc[chunk].BasePtr = TmpPtr;
                TmpPtr += ChunkSize;
            }
            // set next available chunk
            NextAvailChunk = 0;

            return ULM_SUCCESS;
        }
};


// More compact types ...

class MemoryPoolShared_t 
    : public MemoryPool_t <MMAP_SHARED_PROT,
                           MMAP_SHARED_FLAGS,
                           MMAP_SHARED_FLAGS> {};

class MemoryPoolPrivate_t
    : public MemoryPool_t <MMAP_PRIVATE_PROT,
                           MMAP_PRIVATE_FLAGS,
                           MMAP_SHARED_FLAGS> {};
    
#endif /* !_MEMORYPOOL */
