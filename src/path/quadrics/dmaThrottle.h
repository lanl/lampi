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
#define E3_RESET_BCOPY_BLOCK(x)
#define EVENT_BLK_READY(x)	true
#else
#include <elan3/elan3.h>
#endif

#include "internal/malloc.h"
#include "internal/state.h"
#include "util/Lock.h"

#ifdef SHARED_MEMORY
#include "internal/constants.h"
#include "mem/FixedSharedMemPool.h"
extern FixedSharedMemPool SharedMemoryPools;
#endif

#define DMATHROTTLEMAGIC 0x31415928

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
 *
 * if reclaim is true and there are no event blocks on the freelist, dmaThrottle
 * will attempt to check previously allocated event blocks -- and if they are
 * "done" or ready, will substitute dummyDone for the event block using the pointer
 * to the event block pointer stored in locarray[]. If reclaim is true, then
 * eventDone() is the only safe way to determine if an event is "done"...
 */
class dmaThrottle {

    //! eventBlk is used by Elan DMA, ptr is used by freelist algorithm
    union eventBlkSlot {
        volatile E3_Event_Blk eventBlk;
        struct fI {
            void *ptr;
            int magic;
        } freeInfo;
    };

    typedef union eventBlkSlot eventBlkSlot_t;

  private:

     E3_Event_Blk dummyDone;
    bool sharedMemory;          //!< true if dmaThrottle write structures are in shared memory
    bool reclaim;               //!< true if we try to reclaim events
    int concurrentDmas;         //!< the maximum number of concurrent DMAs per rail to allow
    int nrails;                 //!< the number of Quadrics rails to control DMA access to
    Locks *locks;               //!< a lock per rail if we're using shared memory or we're threaded
    eventBlkSlot_t **freep;     //!< pointers to the heads of the freelists (one per rail)
    eventBlkSlot_t **array;     //!< event blocks to be allocated
    volatile E3_Event_Blk ****locarray; //!< pointers to event block pointer locations for reclaim logic

    //! substitute any previously allocated "done" event blocks with dummyDone
    int reclaimEventBlks(int rail) {
        int result = 0;
        for (int i = 0; i < concurrentDmas; i++) {
            volatile E3_Event_Blk **loc = locarray[rail][i];
            if ((loc != 0) && EVENT_BLK_READY(*loc)) {
                volatile E3_Event_Blk *eventBlk = *loc;
                eventBlkSlot_t *slot = (eventBlkSlot_t *) eventBlk;
                /* substitute dummyDone for the "done" event block */
                *loc = &dummyDone;
                /* clear the location to prevent re-reclaiming... */
                 locarray[rail][i] = 0;
                /* put the event block on the free list */
                if (freep[rail]) {
                    slot->freeInfo.ptr = freep[rail];
                } else {
                    slot->freeInfo.ptr = 0;
                }
                slot->freeInfo.magic = DMATHROTTLEMAGIC;
                freep[rail] = slot;
                /* increment the result */
                result++;
            }
        }
        return result;
    }

  public:

    //! the default constructor
    dmaThrottle(bool shared = true, bool reclaimval = true,
                int dmasperrail = 128, int numrails = 2) {
        /* initialize private parameters */
#ifdef SHARED_MEMORY
        sharedMemory = shared;
#else
        sharedMemory = false;
#endif
        reclaim = reclaimval;
        concurrentDmas = dmasperrail;
        nrails = numrails;
        /* get the appropriate memory */
#ifdef SHARED_MEMORY
        if (sharedMemory) {
            locks =
                (Locks *) SharedMemoryPools.getMemorySegment(nrails *
                                                             sizeof(Locks),
                                                             CACHE_ALIGNMENT);
            freep =
                (eventBlkSlot_t **) SharedMemoryPools.
                getMemorySegment(nrails * sizeof(eventBlkSlot_t *),
                                 CACHE_ALIGNMENT);
            array =
                (eventBlkSlot_t **) SharedMemoryPools.
                getMemorySegment(nrails * sizeof(eventBlkSlot_t *),
                                 CACHE_ALIGNMENT);
            for (int j = 0; j < nrails; j++) {
                array[j] =
                    (eventBlkSlot_t *) SharedMemoryPools.
                    getMemorySegment(concurrentDmas *
                                     sizeof(eventBlkSlot_t),
                                     CACHE_ALIGNMENT);
            }
            if (reclaim) {
                locarray =
                    (volatile E3_Event_Blk ****) SharedMemoryPools.
                    getMemorySegment(nrails * sizeof(E3_Event_Blk ***),
                                     CACHE_ALIGNMENT);
                for (int j = 0; j < nrails; j++) {
                    locarray[j] =
                        (volatile E3_Event_Blk ***) SharedMemoryPools.
                        getMemorySegment(concurrentDmas *
                                         sizeof(E3_Event_Blk **),
                                         CACHE_ALIGNMENT);
                }
                for (int i = 0; i < nrails; i++) {
                    for (int j = 0; j < concurrentDmas; j++) {
                        locarray[i][j] = 0;
                    }
                }
            }
        } else {
#endif
            locks = (Locks *) ulm_malloc(nrails * sizeof(Locks));
            freep =
                (eventBlkSlot_t **) ulm_malloc(nrails *
                                               sizeof(eventBlkSlot_t *));
            array =
                (eventBlkSlot_t **) ulm_malloc(nrails *
                                               sizeof(eventBlkSlot_t *));
            for (int j = 0; j < nrails; j++) {
                array[j] =
                    (eventBlkSlot_t *) ulm_malloc(concurrentDmas *
                                                  sizeof(eventBlkSlot_t));
            }
            if (reclaim) {
                locarray =
                    (volatile E3_Event_Blk ****) ulm_malloc(nrails *
                                                            sizeof
                                                            (E3_Event_Blk
                                                             ***));
                for (int j = 0; j < nrails; j++) {
                    locarray[j] =
                        (volatile E3_Event_Blk ***)
                        ulm_malloc(concurrentDmas *
                                   sizeof(E3_Event_Blk **));
                }
                for (int i = 0; i < nrails; i++) {
                    for (int j = 0; j < concurrentDmas; j++) {
                        locarray[i][j] = 0;
                    }
                }
            }
#ifdef SHARED_MEMORY
        }
#endif
        for (int i = 0; i < nrails; i++) {
            /* run the lock constructor */
            new(&locks[i]) Locks;
            /* build the freelist */
            freep[i] = &(array[i][0]);
            for (int j = 0; j < (concurrentDmas - 1); j++) {
                array[i][j].freeInfo.ptr = &(array[i][j + 1]);
                array[i][j].freeInfo.magic = DMATHROTTLEMAGIC;
            }
            array[i][concurrentDmas - 1].freeInfo.ptr = 0;
            array[i][concurrentDmas - 1].freeInfo.magic = DMATHROTTLEMAGIC;
        }

        /* fill in dummyDone with 0xff in all bytes so that it will appear "done" */
        memset((void *) &dummyDone, 0xff, sizeof(E3_Event_Blk));
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
            if (reclaim) {
                for (int j = 0; j < nrails; j++) {
                    free(locarray[j]);
                }
                free(locarray);
            }
        }
    }

    //! returns an pointer to an event block on the proper rail freelist, or 0 if nothing is available
    /*! \param rail the index of the Quadrics rail that this DMA will be posted to
     * \param location a valid address where the event block pointer will be stored
     *
     * location must be a shared memory address if sharedMemory is true and multiple processes are
     * using the "same" dmaThrottle. location can be set to 0 so that the returned event block is
     * not reclaimed; it can be ignored altogether if reclaim is false.
     */
    volatile E3_Event_Blk *getEventBlk(int rail,
                                       volatile E3_Event_Blk ** location =
                                       0) {
        int index;
        volatile E3_Event_Blk *result = 0;
        if (usethreads() || sharedMemory)
            locks[rail].lock();
        if (freep[rail]) {
            result = (volatile E3_Event_Blk *) freep[rail];
            freep[rail]->freeInfo.magic = 0;
            freep[rail] = (eventBlkSlot_t *) freep[rail]->freeInfo.ptr;
            /* reset the event block */
            E3_RESET_BCOPY_BLOCK(result);
            mb();
            /* store the location for reclaim logic */
            if (reclaim) {
#ifdef __i386
                index =
                    (int) ((long) result -
                           (long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#else
                index =
                    (int) ((long long) result -
                           (long long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#endif
                locarray[rail][index] = location;
            }
        } else if (reclaim) {
            if (reclaimEventBlks(rail) > 0) {
                result = (volatile E3_Event_Blk *) freep[rail];
                freep[rail]->freeInfo.magic = 0;
                freep[rail] = (eventBlkSlot_t *) freep[rail]->freeInfo.ptr;
                /* reset the event block */
                E3_RESET_BCOPY_BLOCK(result);
                mb();
                /* store the location for reclaim logic */
#ifdef __i386
                index =
                    (int) ((long) result -
                           (long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#else
                index =
                    (int) ((long long) result -
                           (long long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#endif
                locarray[rail][index] = location;
            }
        }
        if (usethreads() || sharedMemory)
            locks[rail].unlock();
        return result;
    }

    //! returns eventBlk back to the proper rail freelist
    /*! \param rail the index of the Quadrics rail that this DMA was posted to
     * \param eventBlk a pointer to a previously allocated event block from getEventBlk()
     * \param eventBlkLoc a valid address where the event block pointer was stored (see location of getEventBlk())
     *
     * the same caveats apply to eventBlkLoc as getEventBlk's location...
     */
    void freeEventBlk(int rail, volatile E3_Event_Blk * eventBlk,
                      volatile E3_Event_Blk ** eventBlkLoc = 0) {
        int index;
        eventBlkSlot_t *slot = (eventBlkSlot_t *) eventBlk;
        if (!slot)
            return;
        if (usethreads() || sharedMemory)
            locks[rail].lock();
        if (reclaim) {
            /* possibly freed or reclaimed already...if so, return */
            if ((slot->freeInfo.magic == DMATHROTTLEMAGIC)
                || (eventBlk == &dummyDone)) {
                if (usethreads() || sharedMemory)
                    locks[rail].unlock();
                return;
            }
#ifdef __i386
            index =
                (int) ((long) eventBlk -
                       (long) &(array[rail][0])) / sizeof(eventBlkSlot_t);
#else
            index =
                (int) ((long long) eventBlk -
                       (long long) &(array[rail][0])) /
                sizeof(eventBlkSlot_t);
#endif
            /* already reallocated to somebody else */
            if (locarray[rail][index] != eventBlkLoc) {
                if (usethreads() || sharedMemory)
                    locks[rail].unlock();
                return;
            }
        }
        /* put the event block on the free list */
        if (freep[rail]) {
            slot->freeInfo.ptr = freep[rail];
        } else {
            slot->freeInfo.ptr = 0;
        }
        slot->freeInfo.magic = DMATHROTTLEMAGIC;
        freep[rail] = slot;
        /* reset location to a nil value */
        if (reclaim) {
            locarray[rail][index] = 0;
        }
        if (usethreads() || sharedMemory)
            locks[rail].unlock();
    }

    //! safe way to determine if the event block is ready...if reclaiming is being done
    /*! \param rail the index of the Quadrics rail that this DMA has been posted to
     * \param eventBlk a pointer to a previously allocated event block from getEventBlk()
     * \param eventBlkLoc a valid address where the event block pointer was stored (see location of getEventBlk())
     *
     * eventBlkLoc is only necessary if reclaim is true...
     */
    bool eventBlkReady(int rail, volatile E3_Event_Blk * eventBlk,
                       volatile E3_Event_Blk ** eventBlkLoc = 0) {
        int index;
        eventBlkSlot_t *slot = (eventBlkSlot_t *) eventBlk;
        bool result = false;
        if (reclaim) {
            if (usethreads() || sharedMemory)
                locks[rail].lock();
            /* various conditions that indicate that reclaiming has occurred */
            if ((slot->freeInfo.magic == DMATHROTTLEMAGIC)
                || (eventBlk == &dummyDone)) {
                result = true;
            } else {
#ifdef __i386
                index =
                    (int) ((long) slot -
                           (long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#else
                index =
                    (int) ((long long) slot -
                           (long long) &(array[rail][0])) /
                    sizeof(eventBlkSlot_t);
#endif
                if (locarray[rail][index] != eventBlkLoc) {
                    result = true;
                } else {
                    /* event not reclaimed...just check the event block... */
                    result = EVENT_BLK_READY(eventBlk);
                }
            }
            if (usethreads() || sharedMemory)
                locks[rail].unlock();
        } else {
            result = EVENT_BLK_READY(eventBlk);
        }
        return result;
    }

    //! prints the current state of the dmaThrottle object (for debugging purposes)
    void dump(FILE * fp) {
        fprintf(fp,
                "dmaThrottle at %p, sharedMemory = %s, reclaim = %s:\n",
                this, sharedMemory ? "true" : "false",
                reclaim ? "true" : "false");
        fprintf(fp,
                "\tmax quadrics rails = %d, max concurrent dmas per rail = %d\n",
                nrails, concurrentDmas);
        fprintf(fp,
                "\tlocks = %p, freep = %p, array = %p, locarray = %p\n",
                locks, freep, array, locarray);
        for (int j = 0; j < nrails; j++) {
            fprintf(fp, "\tarray[%d] = %p\n", j, array[j]);
        }
        if (locarray) {
            for (int j = 0; j < nrails; j++) {
                fprintf(fp, "\tlocarray[%d] = %p\n", j, locarray[j]);
            }
        }
        for (int i = 0; i < nrails; i++) {
            int freeCnt = 0;
            eventBlkSlot_t *j = freep[i];
            if (j) {
                if (usethreads() || sharedMemory)
                    locks[i].lock();
                fprintf(fp, "Rail %d:\n", i);
                while (j) {
                    fprintf(fp, "\tfreelist eventBlk[%d] = %p, magic %s\n",
                            freeCnt++, j,
                            (j->freeInfo.magic ==
                             DMATHROTTLEMAGIC) ? "correct" : "incorrect");
                    j = (eventBlkSlot_t *) j->freeInfo.ptr;
                }
                if (usethreads() || sharedMemory)
                    locks[i].unlock();
            }
        }
        fflush(fp);
    }
};

#endif
