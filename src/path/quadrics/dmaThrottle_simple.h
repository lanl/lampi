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



#ifndef _DMA_THROTTLE
#define _DMA_THROTTLE

#include <new>
#include <stdio.h>
#include <stdlib.h>

#ifdef DUMMY_ELAN3
typedef struct {int dummy[16];} E3_Event_Blk;
#define E3_RESET_BCOPY_BLOCK(x)
#else
#include <elan3/elan3.h>
#endif

#include "internal/malloc.h"
#include "internal/state.h"
#include "util/Lock.h"

#ifdef ENABLE_SHARED_MEMORY
#include "internal/constants.h"
#include "mem/FixedSharedMemPool.h"
extern FixedSharedMemPool SharedMemoryPools;
#endif

/*! dmaThrottle controls the number of concurrent DMAs to hopefully avoid
 * the dreaded command port overflow of the Elan NIC; it does this by
 * controlling the availability of an event block to be used as a copy
 * target (by a block copy event at the end of an event chain used by
 * our Elan3 DMAs). This is not used by the libelan QDMA support -- but
 * we may be bypassing that in the future for our control queue.
 *
 * dmaThrottle can be instantiated in shared memory (technically only its
 * write variables) for coordination of all the processes on a single
 * machine in a given LAMPI job, or in process-private memory to coordinate
 * the local process' (threaded or not) number of concurrent DMAs.
 *
 * Both the number of rails and the maximum number of concurrent DMAs to
 * allow per rail can be parameters to this object's constructor at
 * run-time.
 */
class dmaThrottle {

    //! eventBlk is used by Elan DMA, ptr is used by freelist algorithm
    union eventBlkSlot {
        volatile E3_Event_Blk eventBlk;
        void *ptr;
    };

    typedef union eventBlkSlot eventBlkSlot_t;

private:

    bool sharedMemory;		//!< true if dmaThrottle write structures are in shared memory
    int concurrentDmas;		//!< the maximum number of concurrent DMAs per rail to allow
    int nrails;			//!< the number of Quadrics rails to control DMA access to
    Locks *locks;			//!< a lock per rail if we're using shared memory or we're threaded
    eventBlkSlot_t **freep;		//!< pointers to the heads of the freelists (one per rail)
    eventBlkSlot_t **array;		//!< event blocks to be allocated

public:

    //! the default constructor
    dmaThrottle(bool shared = true, int dmasperrail = 128, int numrails = 2) {
        /* initialize private parameters */
#ifdef ENABLE_SHARED_MEMORY
        sharedMemory = shared;
#else
        sharedMemory = false;
#endif
        concurrentDmas = dmasperrail;
        nrails = numrails;
        /* get the appropriate memory */
#ifdef ENABLE_SHARED_MEMORY
        if (sharedMemory) {
            locks = (Locks *)SharedMemoryPools.getMemorySegment(
                nrails * sizeof(Locks), CACHE_ALIGNMENT);
            freep = (eventBlkSlot_t **)SharedMemoryPools.getMemorySegment(
                nrails * sizeof(eventBlkSlot_t *), CACHE_ALIGNMENT);
            array = (eventBlkSlot_t **)SharedMemoryPools.getMemorySegment(
                nrails * sizeof(eventBlkSlot_t *), CACHE_ALIGNMENT);
            for (int j = 0 ; j < nrails; j++) {
                array[j] = (eventBlkSlot_t *)SharedMemoryPools.getMemorySegment(
                    concurrentDmas * sizeof(eventBlkSlot_t), CACHE_ALIGNMENT);
            }
        }
        else {
#endif
            locks = (Locks *)ulm_malloc(nrails * sizeof(Locks));
            freep = (eventBlkSlot_t **)ulm_malloc(nrails * sizeof(eventBlkSlot_t *));
            array = (eventBlkSlot_t **)ulm_malloc(nrails * sizeof(eventBlkSlot_t *));
            for (int j = 0 ; j < nrails; j++) {
                array[j] = (eventBlkSlot_t *)ulm_malloc(concurrentDmas * sizeof(eventBlkSlot_t));
            }
#ifdef ENABLE_SHARED_MEMORY
        }
#endif
        for (int i = 0; i < nrails; i++) {
            /* run the lock constructor */
            new (&locks[i]) Locks;
            /* build the freelist */
            freep[i] = &(array[i][0]);
            for (int j = 0; j < (concurrentDmas - 1); j++) {
                array[i][j].ptr = &(array[i][j + 1]);
            }
            array[i][concurrentDmas - 1].ptr = 0;
        }
    }

    //! the default destructor
    ~dmaThrottle() {
        /* we only free memory resources for process-private malloc'ed memory */
        if (!sharedMemory) {
            free(locks);
            free(freep);
            for (int j = 0; j < nrails; j++) {
                free(array[j]);
            }
            free(array);
        }
    }

    //! returns an pointer to an event block on the proper rail freelist, or 0 if nothing is available
    volatile E3_Event_Blk *getEventBlk(int rail) {
        volatile E3_Event_Blk *result = 0;
        if (usethreads() || sharedMemory)
            locks[rail].lock();
        if (freep[rail]) {
            result = (volatile E3_Event_Blk *)freep[rail];
            freep[rail] = (eventBlkSlot_t *)freep[rail]->ptr;
            /* reset the event block */
            E3_RESET_BCOPY_BLOCK(result);
            mb();
        }
        if (usethreads() || sharedMemory)
            locks[rail].unlock();
        return result;
    }

    //! returns eventBlk back to the proper rail freelist
    void freeEventBlk(int rail, volatile E3_Event_Blk *eventBlk) {
        eventBlkSlot_t *slot = (eventBlkSlot_t *)eventBlk;
        if (!slot)
            return;
        if (usethreads() || sharedMemory)
            locks[rail].lock();
        if (freep[rail]) {
            slot->ptr = freep[rail];
            freep[rail] = slot;
        }
        else {
            slot->ptr = 0;
            freep[rail] = slot;
        }
        if (usethreads() || sharedMemory)
            locks[rail].unlock();
    }

    //! prints the current state of the dmaThrottle object (for debugging purposes)
    void dump(FILE *fp) {
        fprintf(fp, "dmaThrottle at %p, sharedMemory = %s:\n", this, sharedMemory ? "true" : "false");
        fprintf(fp, "\tmax quadrics rails = %d, max concurrent dmas per rail = %d\n",
                nrails, concurrentDmas);
        fprintf(fp, "\tlocks = %p, freep = %p, array = %p\n",
                locks, freep, array);
        for (int j = 0; j < nrails; j++) {
            fprintf(fp, "\tarray[%d] = %p\n", j, array[j]);
        }
        for (int i = 0 ; i < nrails; i++) {
            int freeCnt = 0;
            eventBlkSlot_t *j = freep[i];
            if (j) {
                if (usethreads() || sharedMemory)
                    locks[i].lock();
                fprintf(fp,"Rail %d:\n", i);
                while (j) {
                    fprintf(fp,"\tfreelist eventBlk[%d] = %p\n",freeCnt++,j);
                    j = (eventBlkSlot_t *)j->ptr;
                }
                if (usethreads() || sharedMemory)
                    locks[i].unlock();
            }
        }
        fflush(fp);
    }
};

#endif
