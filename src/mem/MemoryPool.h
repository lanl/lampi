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

#ifndef _MEMORYPOOL
#define _MEMORYPOOL

#include "internal/mmap_params.h"
#include "internal/log.h"
#include "internal/new.h"
#include "util/Lock.h"
#include "ulm/ulm.h"
#include "mem/ULMMallocMacros.h"
#include "mem/ULMMallocUtil.h"

#ifdef GM
#include <gm.h>
#endif

//!
//! This routine is used to create and manage a memory pool.
//!   mmap of /dev/zero will be used so that this can be used
//!   to allocate both process private and process shared 
//!   memory.
//!

class PoolBlockData {
public:
    // usage flags
    unsigned short flags;
    // which bucket does this belong to
    int BucketIndex;
    // base pointer
    void *BasePtr;
};

template <int MemProt, int MemFlags, int SharedMemFlag>
class MemoryPool 
{
public:
    //! size of each chunk allocation - in bytes
    unsigned long long ChunkSize;
    
    //! number of chunks
    long NPoolChunks;
    
    //! maximum number of pool chunks
    long maxNPoolChunks;
    
    //! lock
    Locks Lock;
    
    //! page size - needed to adjust ChunkSize to be a multiple of this
    size_t PageSize;
    
    //! array of information about each chunk
    PoolBlockData *ChunkDescriptors;
    
    // next available chunk
    long NextAvailChunk;
   
#ifdef GM
    struct gm_port *gmPort;
#endif

    MemoryPool() 
    { 
#ifdef GM
        gmPort = 0; 
#endif
    }

    // d'tor
    ~MemoryPool()
    {
        if (MemFlags != SharedMemFlag && ChunkDescriptors) {
            ulm_delete(ChunkDescriptors);
            ChunkDescriptors = 0;
        }
    }

    //! request chunk of memory
    void *RequestChunk(int poolIndex)
        {
            void *ReturnPtr=(void *)-1L;
            
            // grab lock on pool
            Lock.lock();
            
            //! Have we used all the allocated memory ?
            if( NextAvailChunk == NPoolChunks ){
                
                //! can we increase the pool size ?  We currently won't grow a shared
                //!  memory region
                if( (MemFlags == SharedMemFlag ) || 
                    ( (maxNPoolChunks > 0) && (maxNPoolChunks == maxNPoolChunks) ) ) {
                    Lock.unlock();
                    return ReturnPtr;
                }
                
                // allocate larger array of chunk descriptors
                PoolBlockData *TmpDesc= ulm_new(PoolBlockData, NPoolChunks+1);
                // copy old array into new array
                for( int desc=0 ; desc < NPoolChunks ; desc++ ) {
                    TmpDesc[desc]=ChunkDescriptors[desc];
                }
                // free old array
                ulm_delete(ChunkDescriptors);
                // set old array pointer to point to new array
                ChunkDescriptors=TmpDesc;
                
                // allocate new memory chunk
                ChunkDescriptors[NPoolChunks].BasePtr=
                    ZeroAlloc(ChunkSize, MMAP_PRIVATE_PROT, MMAP_PRIVATE_FLAGS);
                if(!ChunkDescriptors[NPoolChunks].BasePtr){
                    ulm_err(("Unable to allocate memory for process private "
                             "descriptors \n"));
                    Lock.unlock();
                    return ReturnPtr;
                }
#ifdef GM
                if (gmPort) {
                    gm_status_t returnValue = gm_register_memory(gmPort, 
                        ChunkDescriptors[NPoolChunks].BasePtr, ChunkSize);
                    if (returnValue != GM_SUCCESS) {
                        ulm_err(("Unable to register memory for GM (returnValue = %d)\n",
                            (int)returnValue));
                        return ReturnPtr;
                    }
                }
#endif
                
                // reset pool chunk counter
                NPoolChunks++;
            }
            
            // grab chunk
            ReturnPtr=ChunkDescriptors[NextAvailChunk].BasePtr;
            ChunkDescriptors[NextAvailChunk].flags=ALLOCELEMENT_FLAG_INUSE;
            ChunkDescriptors[NextAvailChunk].BucketIndex=poolIndex;
            
            // find next available chunk
            bool freeChunkFound=false;
            int NextChunkToUse=NextAvailChunk+1;
            while ( NextChunkToUse < NPoolChunks ) {
                
                if ( ChunkDescriptors[NextChunkToUse].flags == ALLOCELEMENT_FLAG_AVAILABLE ){
                    NextAvailChunk=NextChunkToUse;
                    freeChunkFound=true;
                    break;
                }
                
                NextChunkToUse++;
            }
            // if no chunks available set next chunk past end of list so that next
            // time around more memory will be allocated
            if( !freeChunkFound ) {
                NextAvailChunk=NPoolChunks;
            }
            
            // unlock pool
            Lock.unlock();
            
            return ReturnPtr;
        }
    
    //! initialize memory pool
    //! chunkSize is passed in as a page multiple.
    int Init(unsigned long long PoolSize, long long maxLen, 
             long long PoolChunkSize, size_t PgSize)
        {
            // set page size
            PageSize=PgSize;
            if( ( (PageSize/getpagesize())*getpagesize() ) != PageSize ) {
                return ULM_ERR_BAD_PARAM;
            }
            
            
            // set chunksize - multiple of page size
            ChunkSize=((((PoolChunkSize-1)/PageSize)+1)*PageSize);
            if( ChunkSize == 0 ){
                fprintf(stderr, " Chunksize is of length 0\n");
                fflush(stderr);
                return ULM_ERR_BAD_PARAM;
            }
   
            // set upper limit on pool
            if( maxLen < 0 ) {
                // no upper limit on size
                maxNPoolChunks=-1;
            } else {
                maxNPoolChunks=((maxLen-1)/PageSize)+1;
                if( maxNPoolChunks == 0 ){
                    fprintf(stderr, " PoolSize is of length 0\n");
                    fflush(stderr);
                    return ULM_ERR_BAD_PARAM;
                }
            }
      
            // round up pool size to multiple of page size
            PoolSize = ((((PoolSize-1)/ChunkSize)+1)*ChunkSize);
            if( PoolSize == 0 ){
                fprintf(stderr, " PoolSize is of length 0\n");
                fflush(stderr);
                return ULM_ERR_BAD_PARAM;
            }
   
            if( PoolSize < ChunkSize ) {
                fprintf(stderr, "  PoolSize < ChunkSize\n");
                fflush(stderr);
                return ULM_ERR_BAD_PARAM;
            }
   
            char *TmpPtr=0; ssize_t WorkingSize=PoolSize;
            void *PoolBase = 0;
            while( !TmpPtr && WorkingSize ) {
                // add red-zone pages
                ssize_t SizeToAllocate=WorkingSize+2*PageSize;
   
                // allocate memory
                TmpPtr=(char *)ZeroAlloc(SizeToAllocate, MemProt, MemFlags);
   
                if (TmpPtr == 0 )
                    WorkingSize/=2;
   
                // set base pointer
                if(TmpPtr) {
#ifdef GM
                    if (gmPort) {
                        gm_status_t returnValue = gm_register_memory(gmPort, 
                            TmpPtr + PageSize, WorkingSize);
                        if (returnValue != GM_SUCCESS) {
                            ulm_err(("Unable to register memory for GM (returnValue = %d)\n",
                                (int)returnValue));
                            return ULM_ERROR;
                        }
                    }
#endif
                    PoolBase=(void *)(TmpPtr+PageSize);
                }
   
            }
            // reset pool size
            PoolSize=WorkingSize;
            NPoolChunks=((PoolSize-1)/ChunkSize)+1;
            if( (NPoolChunks > maxNPoolChunks) && ( maxNPoolChunks >0 ) ) {
                fprintf(stderr, " NPoolChunks > maxNPoolChunks :: NPoolChunks %ld maxNPoolChunks %ld\n", 
                        NPoolChunks, maxNPoolChunks);
                fflush(stderr);
                return ULM_ERR_BAD_PARAM;
            }
   
            // change memory protection for red zones
            int retval=mprotect(TmpPtr, PageSize, PROT_NONE);
            if( retval != 0 ) {
                ulm_exit((-1, "Error in red zone 1 mprotect\n"));
            }
            // end red zone
            retval=mprotect(TmpPtr+PageSize+WorkingSize, PageSize, PROT_NONE);
            if( retval != 0 ) {
                ulm_exit((-1, "Error in red zone 2 mprotect\n"));
            }
             
            // initialize chunk descriptors
            if( MemFlags == SharedMemFlag ) {
                // shared Memory allocation
                size_t lenToAlloc=sizeof(PoolBlockData) * NPoolChunks;
                ChunkDescriptors=(PoolBlockData *)ZeroAlloc(lenToAlloc, MemProt, MemFlags);
                if( !ChunkDescriptors ) {
                    fprintf(stderr, " Unable to ZeroAlloc memory for ChunkDescriptors\n");
                    fflush(stderr);
                    return ULM_ERROR;
                }
            } else {
                // process private Memory allocation
                ChunkDescriptors = ulm_new(PoolBlockData, NPoolChunks);
                if(! ChunkDescriptors ) {
                    fprintf(stderr, " Unable to allocate memory for ChunkDescriptors\n");
                    fflush(stderr);
                    return ULM_ERROR;
                }
            }
   
            TmpPtr=(char *)PoolBase;
            for (int chunk=0; chunk < NPoolChunks ; chunk++ ) {
                ChunkDescriptors[chunk].flags=ALLOCELEMENT_FLAG_AVAILABLE;
                ChunkDescriptors[chunk].BucketIndex=-1;
                ChunkDescriptors[chunk].BasePtr=TmpPtr;
                TmpPtr+=ChunkSize;
            }
            // set next available chunk
            NextAvailChunk=0;
            
            return ULM_SUCCESS;
        }
};
    
#endif /* !_MEMORYPOOL */
