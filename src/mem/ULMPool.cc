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

#include <stdio.h>
#include <new>

#include "mem/ULMPool.h"
#include "internal/log.h"
#include "internal/new.h"

//! default constructor
ULMMemoryPool::ULMMemoryPool(int Log2MaxPoolSize, int Log2PoolChunkSize,
			     int Log2PageSize, int MProt, int MFlags,
			     ssize_t initPoolSize, bool shared)
{
    // some sanity checks
    if (Log2MaxPoolSize < Log2PoolChunkSize) {
        ulm_exit((-1, "Log2MaxPoolSize (%ld) < Log2PoolChunkSize (%ld)\n",
                  Log2MaxPoolSize, Log2PoolChunkSize));
    }
    if (Log2PoolChunkSize < Log2PageSize) {
        ulm_exit((-1, "Log2PoolChunkSize (%ld) < Log2PageSize (%ld)\n",
                  Log2PoolChunkSize, Log2PageSize));
    }
    if ((1LL << Log2MaxPoolSize) < initPoolSize) {
        ulm_exit((-1, "max Pool size (%ld) < initPoolSize (%ld)\n",
                  (1LL << Log2MaxPoolSize), initPoolSize));
    }
    // initialize variables
    LogBase2PageSize = Log2PageSize;
    LogBase2ChunkSize = Log2PoolChunkSize;
    PageMask = ((1 << Log2PageSize) - 1);
    PoolChunkSize = (1 << Log2PoolChunkSize);
    MemProt = MProt;
    MemFlags = MFlags;
    isShared = shared;

    // initizlize pool chunks
    MaxPoolSize = 1LL << Log2MaxPoolSize;
    MaxNPoolChunks = 1 << (Log2MaxPoolSize - Log2PoolChunkSize);

    // build PoolChunks in shared memory if the pool is to be shared
    if (isShared) {
        // allocate memory
        PoolChunks = (PoolBlocks *) ZeroAlloc(sizeof(PoolBlocks)
                                              * MaxNPoolChunks, MemProt,
                                              MemFlags);

        // run constructors
        for (int i = 0; i < MaxNPoolChunks; i++)
            new(&(PoolChunks[i])) PoolBlocks;
    } else {                    // Pool resides in private memory
        PoolChunks = ulm_new(PoolBlocks, MaxNPoolChunks);
    }

    // initialize chunks
    for (int chunk = 0; chunk < MaxNPoolChunks; chunk++) {
        PoolChunks[chunk].flags = ALLOCELEMENT_FLAG_AVAILABLE;
        PoolChunks[chunk].bucketindex = -1;
        PoolChunks[chunk].IndexLoanedTo = -1;
    }

    // next chunk to grab
    NextAvailChunk = initPoolSize == 0 ? -1 : 0;

    // actaul pool size
    PoolSize = initPoolSize;

    // allocate pool
    int RetVal = InitPool();
    if (RetVal < 0) {
        ulm_err(("Error: Initializing ULMMemoryPool\n"));
        exit(-1);               /// !!!! better termination required.
    }
}

int ULMMemoryPool::InitPool()
{
    int RetVal = 0;

    // round memory size up to nearest poolchunksize
    NPoolChunks = PoolSize / (1 << LogBase2ChunkSize);
    PoolSize = NPoolChunks * (1 << LogBase2ChunkSize);
    long PageSize = 1 << LogBase2PageSize;

    char *TmpPtr = 0;
    ssize_t WorkingSize = PoolSize;
    while (TmpPtr == 0 && WorkingSize > 0) {
        // add red-zone pages
        ssize_t SizeToAllocate = WorkingSize + 2 * PageSize;

        // allocate memory
        TmpPtr = (char *) ZeroAlloc(SizeToAllocate, MemProt, MemFlags);

        if (TmpPtr == 0)
            WorkingSize /= 2;

        // set base pointer
        if (TmpPtr)
            PoolBase = (void *) (TmpPtr + PageSize);
    }

    // reset pool size
    PoolSize = WorkingSize;
    NPoolChunks = PoolSize / (1 << LogBase2ChunkSize);

    // change memory protection for red zones
    int retval;
    if (WorkingSize > 0) {
        retval = mprotect(TmpPtr, PageSize, PROT_NONE);
        if (retval != 0) {
            ulm_exit((-1, "Error in red zone 1 mprotect \n"));
        }
    }
    // end red zone
    if (WorkingSize > 0) {
        retval =
            mprotect(TmpPtr + PageSize + WorkingSize, PageSize, PROT_NONE);
        if (retval != 0) {
            ulm_exit((-1, "Error in red zone 2 mprotect\n"));
        }
    }

    return RetVal;
}


void ULMMemoryPool::DeletePool()
{
    int retval = munmap(PoolBase, PoolSize);
    if (retval == -1) {
        fprintf(stderr, "Error: unmapping Pool memory.\n");
        fflush(stderr);
    }
    if (isShared) {
        retval = munmap(PoolChunks, MaxNPoolChunks * sizeof(PoolBlocks));
        if (retval == -1) {
            fprintf(stderr, "Error: unmapping PoolChunks memory.\n");
            fflush(stderr);
        }
    } else if (PoolChunks != 0) {
        ulm_delete(PoolChunks);
    }
}


int ULMMemoryPool::SetPoolSize(ssize_t initPoolSize)
{
    // sanity check
    if (MaxPoolSize < initPoolSize) {
        ulm_exit((-1, "Error:: Too much memory requested.\n"
                  "Amount requested :: %ld - Upper limit :: %ld\n",
                  (long) initPoolSize, (long) MaxPoolSize));
    }

    int RetVal = 0;

    // round memory size up to nearest poolchunksize
    NPoolChunks = initPoolSize / (1 << LogBase2ChunkSize);
    PoolSize = NPoolChunks * (1 << LogBase2ChunkSize);
    long PageSize = 1 << LogBase2PageSize;

    char *TmpPtr = 0;
    ssize_t WorkingSize = PoolSize;
    while (TmpPtr == 0 && WorkingSize > 0) {
        // add red-zone pages
        ssize_t SizeToAllocate = WorkingSize + 2 * PageSize;

        // allocate memory
        TmpPtr = (char *) ZeroAlloc(SizeToAllocate, MemProt, MemFlags);

        if (TmpPtr == 0)
            WorkingSize /= 2;

        // set base pointer
        if (TmpPtr)
            PoolBase = (void *) (TmpPtr + PageSize);
    }

    // reset pool size
    PoolSize = WorkingSize;
    NPoolChunks = PoolSize / (1 << LogBase2ChunkSize);
    MaxNPoolChunks = NPoolChunks;

    // initialize pool chunks
    for (int chunk = 0; chunk < NPoolChunks; chunk++) {
        PoolChunks[chunk].flags = ALLOCELEMENT_FLAG_AVAILABLE;
        PoolChunks[chunk].bucketindex = -1;
        PoolChunks[chunk].IndexLoanedTo = -1;
    }

    // change memory protection for red zones
    int retval;
    if (WorkingSize > 0) {
        retval = mprotect(TmpPtr, PageSize, PROT_NONE);
        if (retval != 0) {
            ulm_exit((-1, "Error in red zone 1 mprotect\n"));
        }
    }
    // end red zone
    if (WorkingSize > 0) {
        retval =
            mprotect(TmpPtr + PageSize + WorkingSize, PageSize, PROT_NONE);
        if (retval != 0) {
            ulm_exit((-1, "Error in red zone 2 mprotect\n"));
        }
    }

    if (WorkingSize > 0)
        NextAvailChunk = 0;

    return RetVal;
}

// Request new chunk from memory pool
void *ULMMemoryPool::RequestChunk(int BucketIndex, bool IsLoaned,
                                  int LoanedTo)
{
    void *ReturnPtr = (void *) -1L;

    // grab lock on pool
    Lock.lock();

    // return if no available chunks
    if (NextAvailChunk == -1) {
        Lock.unlock();
        return ReturnPtr;
    }
    // grab chunk
    ReturnPtr =
        (void *) ((char *) PoolBase + PoolChunkSize * NextAvailChunk);
    PoolChunks[NextAvailChunk].flags = ALLOCELEMENT_FLAG_INUSE;
    PoolChunks[NextAvailChunk].bucketindex = BucketIndex;

    // list who this chunk is loaned to
    if (IsLoaned) {
        PoolChunks[NextAvailChunk].IndexLoanedTo = LoanedTo;
        PoolChunks[NextAvailChunk].flags |= ALLOCELEMENT_FLAG_LOANED;
    }
    // find next available chunk
    int NextChunkToUse = NextAvailChunk + 1;
    while (NextChunkToUse < NPoolChunks) {

        if (PoolChunks[NextChunkToUse].flags ==
            ALLOCELEMENT_FLAG_AVAILABLE) {
            NextAvailChunk = NextChunkToUse;
            break;
        }

        NextChunkToUse++;
    }

    if (NextChunkToUse >= NPoolChunks)
        NextAvailChunk = -1;
    Lock.unlock();
    return ReturnPtr;
}

// return new chunk to memory pool

int ULMMemoryPool::ReturnChunk(void *ChunkBaseAddress)
{
    // get chunk index
    ptrdiff_t Offset = (char *) ChunkBaseAddress - (char *) PoolBase;
    if (Offset < 0)
        return -1;

    int ChunkIndex = Offset / PoolChunkSize;
    if (ChunkIndex >= MaxNPoolChunks)
        return -1;

    ptrdiff_t IntegralOffset = (ChunkIndex * PoolChunkSize);
    if (Offset != IntegralOffset)
        return -1;

    // reset flags to indicate chunk is available
    PoolChunks[ChunkIndex].flags = ALLOCELEMENT_FLAG_AVAILABLE;
    PoolChunks[ChunkIndex].bucketindex = -1;
    PoolChunks[ChunkIndex].IndexLoanedTo = -1;

    // reset NextChunkToUse pointer
    if (ChunkIndex < NextAvailChunk)
        NextAvailChunk = ChunkIndex;

    return 0;
}
