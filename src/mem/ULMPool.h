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

#ifndef _ULMPool
#define _ULMPool

//#include "internal/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "util/Lock.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "mem/ULMMallocMacros.h"
#include "mem/ULMMallocUtil.h"

//! Fixed-size pool of shared memory
/*!
 *  This class allocates and manages a fixed-size pool of shared
 *  memory.
 */
class ULMMemoryPool
{
public:
    //! Constructor.  Note that all arguments are required
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

    //! Destructor, Unmaps the shared memory
    ~ULMMemoryPool() {  DeletePool(); }

    //! Request a chunk of memory
    void *RequestChunk(int BucketIndex, bool IsLoaned, int LoanedTo);

    //! Returns a chunk of memory to the pool to be reused
    /*
     * Returns a chunk of memory to the pool for reuse.
     * \param ChunkBaseAddress A pointer to the base address of the chunk
     */
    int ReturnChunk(void *ChunkBaseAddress);

    //! Reset the size of the memory pool
    /*!
     * Resets the size of the memory pool.
     * \param initPoolSize New size of the pool
     * Will not allocate more that the predetermined maximum size
     * of the pool.
     */
    int SetPoolSize(ssize_t initPoolSize);

    void *GetPoolBase() {return PoolBase;}

    PoolBlocks *GetPoolChunksPtr(){ return PoolChunks;}

    void SetBucketIndex(int BucketIndex, int ChunkIndex)
        { PoolChunks[ChunkIndex].bucketindex=BucketIndex; }

    int LogBase2PageSize; //!< Log base 2 of a page of memory
    int LogBase2ChunkSize; //!< Size of the memory chuncks
    long PageMask; //!< mask for pages
    long PoolChunkSize; //!< Size of the pool memory chunks
    int MemProt; //!< Memory protections for the pool
    int MemFlags; //!< Memory flags for the pool
    void *PoolBase; //!< base of memory pool
    long long MaxPoolSize; //!< maximum memory in pool
    long long PoolSize; //!< actual pool size
    long MaxNPoolChunks; //!< maximum number of chunks
    bool isShared; //!< flag indicating if pool exist in shared mem

    // information about each chunk
    PoolBlocks *PoolChunks;

    long NPoolChunks; // number of chunks
    int NextAvailChunk;	// next available chunk

    Locks Lock; //!< Lock for pool

    //! Delete memory pool
    void DeletePool();

private:
    //! Allocate memory pool
    int InitPool();
};

#endif /* _ULMPool */
