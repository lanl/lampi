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

#ifndef _ULMPool
#define _ULMPool

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "util/Lock.h"
#include "internal/types.h"
#include "client/daemon.h"
#include "mem/ULMMallocMacros.h"
#include "mem/PoolChunks.h"

// Fixed-size pool of shared memory
/*!
 *  This class allocates and manages a fixed-size pool of shared
 *  memory.
 */
class ULMMemoryPool
{
public:
    // Constructor.  Note that all arguments are required
    /*
     * Constructor - creates the shared memory pool which other objects
     * use.
     * \param Log2MaxPoolSize Log base 2 of the maximum allowable size of
     * the pool, in bytes
     * \param Log2PoolChunkSize Log base 2 of the size, in bytes, of a
     * chunk of memory which can be obtained from the pool.
     * \param Log2PageSize Log base 2 of the size, in bytes, of a page
     * of memory
     * \param MProt Memory protections to be used for the shared memory
     * \param MFlags Memory flags to be used with the shared memory
     * \param initPoolSize Initial size, in bytes, of the memory pool
     */
    ULMMemoryPool(int Log2MaxPoolSize, int Log2PoolChunkSize,
                  int Log2PageSize, int MProt, int MFlags,
                  ssize_t initPoolSize, bool shared = true);

    // Destructor, Unmaps the shared memory
    ~ULMMemoryPool() {  DeletePool(); }

    // Request a chunk of memory
    void *RequestChunk(int BucketIndex, bool IsLoaned, int LoanedTo);

    // Returns a chunk of memory to the pool to be reused
    /*
     * Returns a chunk of memory to the pool for reuse.
     * \param ChunkBaseAddress A pointer to the base address of the chunk
     */
    int ReturnChunk(void *ChunkBaseAddress);

    // Reset the size of the memory pool
    /*!
     * Resets the size of the memory pool.
     * \param initPoolSize New size of the pool
     * Will not allocate more that the predetermined maximum size
     * of the pool.
     */
    int SetPoolSize(ssize_t initPoolSize);

    void *GetPoolBase() {return PoolBase;}

    PoolChunks_t *GetPoolChunksPtr(){ return PoolChunks;}

    void SetBucketIndex(int BucketIndex, int ChunkIndex)
        { PoolChunks[ChunkIndex].bucketindex=BucketIndex; }

    int LogBase2PageSize;           // Log base 2 of a page of memory
    int LogBase2ChunkSize;          // Size of the memory chuncks
    long PageMask;                  // mask for pages
    long PoolChunkSize;             // Size of the pool memory chunks
    int MemProt;                    // Memory protections for the pool
    int MemFlags;                   // Memory flags for the pool
    void *PoolBase;                 // base of memory pool
    long long MaxPoolSize;          // maximum memory in pool
    long long PoolSize;             // actual pool size
    long MaxNPoolChunks;            // maximum number of chunks
    bool isShared;                  // flag indicating if pool exist in shared mem
    PoolChunks_t *PoolChunks;       // information about each chunk
    long NPoolChunks;               // number of chunks
    int NextAvailChunk;             // next available chunk
    Locks Lock;                     // Lock for pool

    // Delete memory pool
    void DeletePool();

private:
    // Allocate memory pool
    int InitPool();
};

#endif /* _ULMPool */
