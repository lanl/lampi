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
#include <string.h>

#ifdef DUMMY_ELAN3
typedef struct {
    int dummy[16];
} E3_Event_Blk;
#define elan3_allocMain(x,y,z) ulm_malloc(z)
#define elan3_free(x,y) free(y)
#define E3_RESET_BCOPY_BLOCK(x)
#define EVENT_BLK_READY(x)	true
#else
#include <elan3/elan3.h>
ELAN3_CTX *getElan3Ctx(int rail);
#endif

#include "internal/malloc.h"
#include "internal/state.h"
#include "util/Lock.h"
#include "path/quadrics/state.h"

#ifdef ENABLE_SHARED_MEMORY
#include "internal/constants.h"
#include "queue/globals.h"
#include "mem/FixedSharedMemPool.h"
extern FixedSharedMemPool SharedMemoryPools;
#endif

#define DMATHROTTLEMAGIC 0x31415928

/*! dmaThrottle controls the number of concurrent DMAs to hopefully avoid
 * the dreaded command port overflow of the Elan NIC; it does this by
 * controlling the availability of an event block to be used as a copy
 * target (by a block copy event at the end of an event chain used by
 * our Elan3 DMAs).
 *
 * dmaThrottle can be instantiated in shared memory (technically only some of its
 * write variables) for coordination of all the processes on a single
 * machine in a given LAMPI job, or in process-private memory to coordinate
 * the local process' (threaded or not) number of concurrent DMAs.
 *
 * Both the number of rails and the maximum number of concurrent DMAs to
 * allow per rail can be parameters to this object's constructor at
 * run-time.
 *
 * if reclaim is true and there are no event blocks on the freelist, dmaThrottle
 * will attempt to check previously allocated event blocks for this process -- and if they are
 * "done" or ready, will substitute dummyDone[rail] for the event block using the pointer
 * to the event block pointer stored at loc. If reclaim is true, then
 * eventDone() is the only safe way to determine if an event is "done"...
 */
class dmaThrottle {

    //! eventBlk is used by Elan DMA, ptr is used by freelist algorithm
    struct eventBlkSlot {
	volatile E3_Event_Blk *eventBlk;
	volatile E3_Event_Blk **loc;
	struct eventBlkSlot *ptr;
	int magic;
    };

    typedef struct eventBlkSlot eventBlkSlot_t;

    struct allocPtrs {
        struct eventBlkSlot *head;
        struct eventBlkSlot *tail;
    };

    typedef struct allocPtrs allocPtrs_t;

private:

    E3_Event_Blk **dummyDone;
    bool sharedMemory;		//!< true if dmaThrottle write structures are in shared memory
    bool reclaim;		//!< true if we try to reclaim events
    int concurrentDmas;		//!< the maximum number of concurrent DMAs per rail to allow
    int nrails;			//!< the number of Quadrics rails to control DMA access to
    int *dmaCounts;		//!< the number of current DMAs for a given rail
    Locks *locks;		//!< a lock per rail if we're using shared memory or we're threaded
    eventBlkSlot_t **freep;	//!< pointers to the heads of the freelists (one per rail)
    allocPtrs_t *allocp;	//!< pointers to head and tail of the allocated lists (one per rail)
    eventBlkSlot_t **array;	//!< event info blocks to be allocated

    //! substitute any previously allocated "done" event blocks with dummyDone
    int reclaimEventBlks(int rail) {
	int result = 0;
	eventBlkSlot_t *slot = allocp[rail].head;
    eventBlkSlot_t *prev = 0;
	while (slot) {
            eventBlkSlot_t *next = slot->ptr;
            volatile E3_Event_Blk **loc =slot->loc;
            if ((loc != 0) && EVENT_BLK_READY(*loc)) {
                /* substitute dummyDone[rail] for the "done" event block */
                *loc = dummyDone[rail];
                /* clear the location to prevent re-reclaiming... */
                slot->loc = 0;
                /* put the event block on the free list */
                if (freep[rail]) {
                    slot->ptr = freep[rail];
                } else {
                    slot->ptr = 0;
                }
                slot->magic = DMATHROTTLEMAGIC;
                freep[rail] = slot;
                /* decrease the current number of DMAs */
                dmaCounts[rail]--;
                /* increment the result */
                result++;
                /* move slot off of allocp[rail] list */
                if (prev)
                    prev->ptr = next;
                if (slot == allocp[rail].head)
                    allocp[rail].head = next;
                if (slot == allocp[rail].tail)
                    allocp[rail].tail = prev;
            }
            else {
                prev = slot;
            }
            slot = next;
        }
        return result;
    }

public:

    //! the default constructor
    dmaThrottle(bool shared = true, bool reclaimval = true,
		int dmasperrail = 128, int numrails = 2) {
	/* initialize private parameters */
#ifdef ENABLE_SHARED_MEMORY
	sharedMemory = shared;
#else
	sharedMemory = false;
#endif
	reclaim = reclaimval;
	concurrentDmas = dmasperrail;
	nrails = numrails;

	// don't ever use shared memory if we are the only local process
	if (local_nprocs() == 1)
            sharedMemory = false;

	/* get the appropriate memory */
#ifdef ENABLE_SHARED_MEMORY
	if (sharedMemory) {
            locks = (Locks *) SharedMemoryPools.getMemorySegment(nrails * sizeof(Locks),
                                                                 CACHE_ALIGNMENT);
            dmaCounts = (int *)SharedMemoryPools.getMemorySegment(nrails * sizeof(int),
                                                                  CACHE_ALIGNMENT);
	} else {
#endif
            locks = (Locks *) ulm_malloc(nrails * sizeof(Locks));
            dmaCounts = (int *)ulm_malloc(nrails * sizeof(int));
#ifdef ENABLE_SHARED_MEMORY
	}
#endif
    }


    void postQuadricsInit() {
	freep = (eventBlkSlot_t **) ulm_malloc(nrails * sizeof(eventBlkSlot_t *));
	allocp = (allocPtrs_t *) ulm_malloc(nrails * sizeof(allocPtrs_t));
	array = (eventBlkSlot_t **) ulm_malloc(nrails * sizeof(eventBlkSlot_t *));
	dummyDone = (E3_Event_Blk **) ulm_malloc(nrails * sizeof(E3_Event_Blk *));
	for (int j = 0; j < nrails; j++) {
            allocp[j].head = allocp[j].tail = 0;
            array[j] = (eventBlkSlot_t *) ulm_malloc(concurrentDmas * sizeof(eventBlkSlot_t));
            dummyDone[j] = (E3_Event_Blk *)elan3_allocMain(getElan3Ctx(j), E3_BLK_ALIGN, sizeof(E3_Event_Blk));
	}

	/* initialize structures */
	for (int i = 0; i < nrails; i++) {
            /* clear dmaCounts[rail] */
            dmaCounts[i] = 0;
	    /* run the lock constructor */
	    new(&locks[i]) Locks;
	    /* build the freelist */
	    freep[i] = &(array[i][0]);
	    for (int j = 0; j < (concurrentDmas - 1); j++) {
		array[i][j].eventBlk = 0;
		array[i][j].loc = 0;
		array[i][j].ptr = &(array[i][j + 1]);
		array[i][j].magic = DMATHROTTLEMAGIC;
	    }
	    array[i][concurrentDmas - 1].eventBlk = 0;
	    array[i][concurrentDmas - 1].loc = 0;
	    array[i][concurrentDmas - 1].ptr = 0;
	    array[i][concurrentDmas - 1].magic = DMATHROTTLEMAGIC;
	    /* fill in dummyDone with 0xff in all bytes so that it will appear "done" */
	    memset((void *)dummyDone[i], 0xff, sizeof(E3_Event_Blk));
	}
    }

    //! the default destructor
    ~dmaThrottle() {
	/* we only free memory resources for process-private malloc'ed memory */
	if (!sharedMemory) {
            free(locks);
            free(dmaCounts);
	}
	free(freep);
	free(allocp);
    if ( usethreads() )
        quadricsState.quadricsLock.lock();

    for (int j = 0; j < nrails; j++) {
            free(array[j]);
            elan3_free(getElan3Ctx(j), dummyDone[j]);
	}
    if ( usethreads() )
        quadricsState.quadricsLock.unlock();

    free(array);
	free(dummyDone);
    }

    //! returns an pointer to an event block on the proper rail freelist, or 0 if nothing is available
    /*! \param rail the index of the Quadrics rail that this DMA will be posted to
     * \param location a valid address where the event block pointer will be stored
     *
     * location must be a shared memory address if sharedMemory is true and multiple processes are
     * using the "same" dmaThrottle. location can be set to 0 so that the returned event block is
     * not reclaimed; it can be ignored altogether if reclaim is false.
     */
    volatile E3_Event_Blk *getEventBlk(int rail, volatile E3_Event_Blk ** location = 0) {
	volatile E3_Event_Blk *result = 0;
	eventBlkSlot_t *slot;
	if (usethreads() || sharedMemory)
    {
        quadricsState.quadricsLock.lock();
        locks[rail].lock();
    }

    if (dmaCounts[rail] < concurrentDmas) {
	    slot = freep[rail];
	    result = (slot->eventBlk) ? slot->eventBlk :
                (volatile E3_Event_Blk *)elan3_allocMain(getElan3Ctx(rail),
                                                E3_BLK_ALIGN, sizeof(E3_Event_Blk));
	    if (!result) {
            if (usethreads() || sharedMemory)
            {
                locks[rail].unlock();
                quadricsState.quadricsLock.unlock();
            }
            return result;
        }
	    dmaCounts[rail]++;
	    slot->eventBlk = result;
	    slot->loc = location;
	    slot->magic = 0;
	    freep[rail] = slot->ptr;

	    /* put it on the allocated list */
	    if (!allocp[rail].head) {
		allocp[rail].head = allocp[rail].tail = slot;
		slot->ptr = 0;
	    }
	    else {
		allocp[rail].tail->ptr = slot;
		slot->ptr = 0;
		allocp[rail].tail = slot;
	    }
	    /* reset the event block */
	    E3_RESET_BCOPY_BLOCK(result);
        mb();
	}
	else if (reclaim) {
            if (reclaimEventBlks(rail) > 0) {
                slot = freep[rail];
                result = (slot->eventBlk) ? slot->eventBlk :
                    (volatile E3_Event_Blk *)elan3_allocMain(getElan3Ctx(rail),
                                                    E3_BLK_ALIGN, sizeof(E3_Event_Blk));
                if (!result) {
                    if (usethreads() || sharedMemory)
                    {
                        locks[rail].unlock();
                        quadricsState.quadricsLock.unlock();
                    }
                    return result;
                }
                dmaCounts[rail]++;
                slot->eventBlk = result;
                slot->loc = location;
                slot->magic = 0;
                freep[rail] = slot->ptr;
                slot->ptr = 0;
                /* put it on the allocated list */
                if (!allocp[rail].head) {
                    allocp[rail].head = allocp[rail].tail = slot;
                }
                else {
                    allocp[rail].tail->ptr = slot;
                    allocp[rail].tail = slot;
                }
                /* reset the event block */
                E3_RESET_BCOPY_BLOCK(result);
                mb();
            }
	}
	if (usethreads() || sharedMemory)
    {
        locks[rail].unlock();
        quadricsState.quadricsLock.unlock();
    }
    return result;
    }

    //! returns eventBlk back to the proper rail freelist
    /*! \param rail the index of the Quadrics rail that this DMA was posted to
     * \param eventBlk a pointer to a previously allocated event block from getEventBlk()
     * \param eventBlkLoc a valid address where the event block pointer was stored (see location of getEventBlk())
     *
     * the same caveats apply to eventBlkLoc as getEventBlk's location...
     */
    void freeEventBlk(int rail, volatile E3_Event_Blk * eventBlk, volatile E3_Event_Blk ** eventBlkLoc = 0) {
        eventBlkSlot_t *prev = 0;
        eventBlkSlot_t *slot;

        if (!eventBlk)
	    return;
	if (usethreads() || sharedMemory)
    {
        quadricsState.quadricsLock.lock();
        locks[rail].lock();
    }

    slot = allocp[rail].head;

    /* find right slot */
    while (slot) {
            if (slot->eventBlk == eventBlk) {
                if (reclaim) {
                    /* possibly freed, reclaimed already, or reallocated already...if so, return */
                    if ((slot->magic == DMATHROTTLEMAGIC) || (eventBlk == dummyDone[rail])
                        || (slot->loc != eventBlkLoc)) {
                        if (usethreads() || sharedMemory)
                        {
                            locks[rail].unlock();
                            quadricsState.quadricsLock.unlock();
                        }
                        return;
                    }
                }
                if (prev)
                    prev->ptr = slot->ptr;
                if (slot == allocp[rail].head)
                    allocp[rail].head = slot->ptr;
                if (slot == allocp[rail].tail)
                    allocp[rail].tail = prev;
                break;
            }
            prev = slot;
            slot = slot->ptr;
	}
	if (!slot) {
        if (usethreads() || sharedMemory)
        {
            locks[rail].unlock();
            quadricsState.quadricsLock.unlock();
        }
        return;
    }

	/* put the event block on the free list */
	if (freep[rail]) {
	    slot->ptr = freep[rail];
	} else {
	    slot->ptr = 0;
	}
	slot->loc = 0;
	slot->magic = DMATHROTTLEMAGIC;
	freep[rail] = slot;
	dmaCounts[rail]--;
	if (usethreads() || sharedMemory)
    {
        locks[rail].unlock();
        quadricsState.quadricsLock.unlock();
    }
    }

    //! safe way to determine if the event block is ready...if reclaiming is being done
    /*! \param rail the index of the Quadrics rail that this DMA has been posted to
     * \param eventBlk a pointer to a previously allocated event block from getEventBlk()
     * \param eventBlkLoc a valid address where the event block pointer was stored (see location of getEventBlk())
     *
     * eventBlkLoc is only necessary if reclaim is true...
     */
    bool eventBlkReady(int rail, volatile E3_Event_Blk * eventBlk, volatile E3_Event_Blk **eventBlkLoc = 0) {
        bool result = false;
        eventBlkSlot_t *slot;

        if (reclaim) {
            if (usethreads() || sharedMemory)
            {
                quadricsState.quadricsLock.lock();
                locks[rail].lock();
            }
            slot = allocp[rail].head;
            while (slot) {
                if (slot->eventBlk == eventBlk)
                    break;
                slot = slot->ptr;
            }
            /* various conditions that indicate that reclaiming has occurred */
            if (!slot || (slot->magic == DMATHROTTLEMAGIC) || (eventBlk == dummyDone[rail])
                || (slot->loc != eventBlkLoc)) {
                result = true;
            } else {
                /* event not reclaimed...just check the event block... */
                result = EVENT_BLK_READY(eventBlk);
            }
            if (usethreads() || sharedMemory)
            {
                locks[rail].unlock();
                quadricsState.quadricsLock.unlock();
            }
        }
        else {
            result = EVENT_BLK_READY(eventBlk);
        }
        return result;
    }

    //! prints the current state of the dmaThrottle object (for debugging purposes)
    void dump(FILE * fp) {
	fprintf(fp, "dmaThrottle at %p, sharedMemory = %s, reclaim = %s:\n", this,
		sharedMemory ? "true" : "false", reclaim ? "true" : "false");
	fprintf(fp, "\tmax quadrics rails = %d, max concurrent dmas per rail = %d\n", nrails,
		concurrentDmas);
	fprintf(fp, "\tlocks = %p, freep = %p, array = %p\n", locks, freep, array);
	for (int j = 0; j < nrails; j++) {
	    fprintf(fp, "\tarray[%d] = %p\n", j, array[j]);
	}
	for (int i = 0; i < nrails; i++) {
	    int freeCnt = 0;
        eventBlkSlot_t *j;

        if (usethreads() || sharedMemory)
            locks[i].lock();
        j = freep[i];
        if (j) {
            if (usethreads() || sharedMemory)
		    locks[i].lock();
		fprintf(fp, "Rail %d:\n", i);
		while (j) {
		    fprintf(fp, "\tfreelist slot %p eventBlk[%d] = %p, loc = %p, magic %s\n", j, freeCnt++, j->eventBlk,
                            j->loc, (j->magic == DMATHROTTLEMAGIC) ? "correct" : "incorrect");
		    j = (eventBlkSlot_t *) j->ptr;
		}
	    }
        if (usethreads() || sharedMemory)
            locks[i].unlock();
	}
	fflush(fp);
    }
};

#endif
