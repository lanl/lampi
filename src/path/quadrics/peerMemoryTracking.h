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

#ifndef _PEER_MEMORY_TRACKING
#define _PEER_MEMORY_TRACKING

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "internal/malloc.h"
#include "internal/state.h"
#include "util/dclock.h"
#include "util/Lock.h"

enum bufTrackingTypes {
    PRIVATE_SMALL_BUFFERS,
    PRIVATE_LARGE_BUFFERS,
    NUMBER_BUFTYPES	/* must be last */
};

/* addrLifo is a last-in-first-out queue that is
 * capable of dynamically growing and shrinking.
 */

class addrLifo {

private:

    int nentries;		//!< number of active entries
    int growBy;		//!< number of entries to grow baseArray by
    int shrinkBy;		//!< number of entries to shrink baseArray by
    int arraySize;		//!< the number of entries baseArray can hold
    void **baseArray;	//!< storage for the LIFO

public:

    //! the default constructor
    addrLifo(int size = 0, int grow = 10, int shrink = 20) {
        baseArray = 0;
        nentries = 0;
        arraySize = size;
        growBy = grow;
        shrinkBy = shrink;
        if (size) {
            baseArray = (void **)ulm_malloc(sizeof(void *)*size);
            if (!baseArray)
                arraySize = 0;
        }
    }

    //! the default destructor
    ~addrLifo() {
        if (baseArray)
            free(baseArray);
    }

    //! returns the current number of active entries in the LIFO
    int size() {
        return nentries;
    }

    //! stores an address (void *) in the LIFO
    bool push(void *addr) {
        bool result = false;
        if ((nentries + 1) <= arraySize) {
            baseArray[nentries++] = addr;
            result = true;
        }
        else if (growBy) {
            void **newp = (void **)realloc(baseArray,(arraySize + growBy)*sizeof(void *));
            if (newp) {
                arraySize += growBy;
                baseArray = newp;
                baseArray[nentries++] = addr;
                result = true;
            }
        }
        return result;
    }

    //! stores cnt addresses (void *) in the LIFO
    bool push(void **addrs, int cnt) {
        bool result = true;
        for (int i = 0; i < cnt; i++) {
            if (!push(addrs[i])) {
                result = false;
                /* pop anything previously pushed -- all or nothing! */
                for (int j = 0; j < i; j++) {
                    pop();
                }
                break;
            }
        }
        return result;
    }

    //! returns true if addr is currently stored in the LIFO
    bool isPushed(void *addr) {
        bool result = false;
        for (int i = 0; i < nentries; i++) {
            if (baseArray[i] == addr) {
                result = true;
                break;
            }
        }
        return result;
    }

    /*! retrieve address from LIFO freelist to reuse the most used addresses for
     * better cache behavior on the peer's side
     */
    void *pop() {
        void *result = 0;
        if (!nentries)
            return result;
        result = baseArray[--nentries];
        if (shrinkBy && ((arraySize - nentries) >= shrinkBy)) {
            void **newp = (void **)realloc(baseArray,(arraySize - shrinkBy)*sizeof(void *));
            if (newp || ((arraySize - shrinkBy) == 0)) {
                arraySize -= shrinkBy;
                baseArray = newp;
            }
        }
        return result;
    }

    /*! retrieve cnt addresses from LIFO freelist to reuse the most used addresses for
     * better cache behavior on the peer's side
     */
    int pop(void **array, int cnt) {
        int result = 0;
        for (int i = 0; i < cnt; i++) {
            if (!nentries) {
                break;
            }
            array[result++] = baseArray[--nentries];
        }
        if (shrinkBy && ((arraySize - nentries) >= shrinkBy)) {
            void **newp = (void **)realloc(baseArray,(arraySize - shrinkBy)*sizeof(void *));
            if (newp || ((arraySize - shrinkBy) == 0)) {
                arraySize -= shrinkBy;
                baseArray = newp;
            }
        }
        return result;
    }

    /*! retrieve up to count addresses in array from the LIFO freelist, grabs
     * the addresses stored on the list for the longest time
     */
    int popLRU(void **array, int count) {
        int result = (nentries) ? count : 0;
        result = (nentries >= result) ? result : nentries;
        for (int i = 0; i < result; i++) {
            array[i] = baseArray[i];
            nentries--;
        }
        if (result) {
            /* repack the LIFO array */
            int stopIndex = result + nentries;
            for (int i = 0, j = result; j < stopIndex; i++, j++) {
                baseArray[i] = baseArray[j];
            }
            if (shrinkBy && ((arraySize - nentries) >= shrinkBy)) {
                void **newp = (void **)realloc(baseArray,(arraySize - shrinkBy)*sizeof(void *));
                if (newp || ((arraySize - shrinkBy) == 0)) {
                    arraySize -= shrinkBy;
                    baseArray = newp;
                }
            }
        }
        return result;
    }

    //! print contents of the LIFO (for debugging purposes)
    void dump(FILE *fp, char *prefix) {
        fprintf(fp, "%snentries = %d, growBy = %d, shrinkBy = %d, arraySize = %d, baseArray = %p\n",
                prefix, nentries, growBy, shrinkBy, arraySize, baseArray);
        for (int i=0; i < nentries; i++) {
            fprintf(fp, "%sslot[%d] = %p\n", prefix, i, baseArray[i]);
        }
    }
};

class peerMemoryTracker {

    //! information for a given destination/peer process
    struct peerInfo {

        //! pointer to a possibly chained next peerInfo structure
        struct peerInfo *next;

        //! destination global process id
        int destination;

        //! dclock() time at initialization
        double time_started;

        /*! array of times that we last updated the weighted
         * time averaged freelist size
         */
        double time_lastupdated[NUMBER_BUFTYPES];

        /*! array of weighted time averaged freelist sizes (one
         * per buffer type), initialized to zero, calculated as
         * Snew = ((Tnow - Tlastupdate)*Scurrent +
         * (Tlastupdate - Tstart)*Savg) / (Tnow - Tstart)
         */
        double time_avg_freelist_size[NUMBER_BUFTYPES];

        /*! dynamic growable/shrinkable LIFOs to track peer
         * memory buffer addresses
         */
        addrLifo lists[NUMBER_BUFTYPES];
    };

private:

    //! array of size 2^N for indexing
    struct peerInfo **tracker;

    //! size of tracker array
    long int slots;

    //! mask bit-ANDed to global destination ID for index
    long int mask;

    //! boolean used to control freelist statistics calculation
    bool calcStats;

    //! thread lock - init'ed by the constructor
    Locks lock;

    /*! creates a new peerInfo structure for a given destination,
     * initializes the entry, and inserts it properly into
     * the tracker indexing structures
     */
    struct peerInfo *create(int destination) {
        struct peerInfo *p = new peerInfo;
        struct peerInfo *prevp = 0;
        struct peerInfo *nextp = tracker[destination & mask];
        if (!p)
            return p;
        p->next = 0;
        p->destination = destination;
        if (calcStats) {
            double now = dclock();
            p->time_started = now;
            for (int i = 0; i < NUMBER_BUFTYPES; i++) {
                p->time_lastupdated[i] = now;
                p->time_avg_freelist_size[i] = 0.0;
            }
        }
        /* insert p at the right point */
        if (!nextp) {
            tracker[destination & mask] = p;
        }
        else {
            /* try to insert p, ordered by destination */
            while (nextp) {
                if (destination < nextp->destination) {
                    p->next = nextp;
                    if (prevp)
                        prevp->next = p;
                    else
                        tracker[destination & mask] = p;
                    break;
                }
                prevp = nextp;
                nextp = nextp->next;
            }
            /* just append p to the end of this bucket's chain */
            if ((nextp == 0) && prevp) {
                prevp->next = p;
            }
        }
        return p;
    }

public:

    //! the default constructor
    peerMemoryTracker(int sizeExponent = 8, bool calculateStats = false) {
        calcStats = calculateStats;
        slots = (1 << sizeExponent);
        mask = slots - 1;
        tracker = (struct peerInfo **)ulm_malloc(sizeof(struct peerInfo *) * slots);
        assert(tracker != 0);
        for (int i = 0; i < slots; i++) {
            tracker[i] = 0;
        }
    }

    //! the default destructor
    ~peerMemoryTracker() {
        struct peerInfo *info;
        for (int i = 0; i < slots ; i++) {
            info = tracker[i];
            while (info) {
                struct peerInfo *next = info->next;
                delete info;
                info = next;
            }
        }
        delete tracker;
    }

    //! store an address on the proper freelist (returns true if successful)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (process-private v. shared, etc.) of addr
     * \param addr the address to be stored
     */
    bool push(int destination, int bufType, void *addr) {
        bool result = false;
        if (usethreads())
            lock.lock();
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        /* found it! */
        if (info) {
            if (calcStats) {
                double now = dclock();
                double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                    info->time_avg_freelist_size[bufType];
                info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                    (a0 + a1)/(now - info->time_started);
                info->time_lastupdated[bufType] = now;
            }
            result = info->lists[bufType].push(addr);
        }
        /* or...create a peerInfo structure if necessary */
        else {
            info = create(destination);
            if (info) {
                if (calcStats) {
                    double now = dclock();
                    double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                    double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                        info->time_avg_freelist_size[bufType];
                    info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                        (a0 + a1)/(now - info->time_started);
                    info->time_lastupdated[bufType] = now;
                }
                result = info->lists[bufType].push(addr);
            }
        }
        if (usethreads())
            lock.unlock();
        return result;
    }

    //! store cnt addresses on the proper freelist (returns true if successful for all)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (process-private v. shared, etc.) of addr
     * \param addrs an array of the addresses to be stored
     * \param cnt the number of addresses in addrs
     * if this routine does not succeed (returns false), then none of the addresses
     * will have been stored.
     */
    bool push(int destination, int bufType, void **addrs, int cnt) {
        bool result = false;
        if (usethreads())
            lock.lock();
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        /* found it! */
        if (info) {
            if (calcStats) {
                double now = dclock();
                double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                    info->time_avg_freelist_size[bufType];
                info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                    (a0 + a1)/(now - info->time_started);
                info->time_lastupdated[bufType] = now;
            }
            result = info->lists[bufType].push(addrs, cnt);
        }
        /* or...create a peerInfo structure if necessary */
        else {
            info = create(destination);
            if (info) {
                if (calcStats) {
                    double now = dclock();
                    double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                    double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                        info->time_avg_freelist_size[bufType];
                    info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                        (a0 + a1)/(now - info->time_started);
                    info->time_lastupdated[bufType] = now;
                }
                result = info->lists[bufType].push(addrs, cnt);
            }
        }
        if (usethreads())
            lock.unlock();
        return result;
    }

    //! is addr currently stored on the proper freelist (returns true if it is)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (small v. large, etc.) of addr
     * \param addr the address to be checked
     */
    bool isPushed(int destination, int bufType, void *addr) {
        bool result = false;
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        if (info)
            result = info->lists[bufType].isPushed(addr);
        return result;
    }

    //! retrieve the last-stored address of type bufType for a peer (returns 0 if nothing is available)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (small v. large, etc.)
     */
    void *pop(int destination, int bufType) {
        void *result = 0;
        if (usethreads())
            lock.lock();
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        if (info) {
            if (calcStats) {
                double now = dclock();
                double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                    info->time_avg_freelist_size[bufType];
                info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                    (a0 + a1)/(now - info->time_started);
                info->time_lastupdated[bufType] = now;
            }
            result = info->lists[bufType].pop();
        }
        if (usethreads())
            lock.unlock();
        return result;
    }

    //! retrieve cnt last-stored addresses of type bufType for a peer (returns 0 if nothing is available)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (small v. large, etc.)
     * \param array the array to store the retrieved addresses in
     * \param cnt the number of addresses to retrieve, if possible
     */
    int pop(int destination, int bufType, void **array, int cnt) {
        int result = 0;
        if (usethreads())
            lock.lock();
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        if (info) {
            if (calcStats) {
                double now = dclock();
                double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                    info->time_avg_freelist_size[bufType];
                info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                    (a0 + a1)/(now - info->time_started);
                info->time_lastupdated[bufType] = now;
            }
            result = info->lists[bufType].pop(array, cnt);
        }
        if (usethreads())
            lock.unlock();
        return result;
    }

    //! retrieve up to maxCnt longest-stored addresses (returns the number of addresses actually retrieved)
    /*! \param destination the global process id of the destination/peer process
     * \param bufType the memory buffer type (small v. large, etc.)
     * \param maxCnt the maximum number of addresses that can be stored in array
     * \param array the array in which to store the retrieved addresses
     * \param forced if set to true, then retrieve up to maxCnt addresses no matter what...
     * this routine will only return addresses if calcStats has been initialized to true, unless
     * forced is set to true (in which case maxCnt is taken to be the desired number of addresses).
     * the routine will pop the N longest-stored addresses, where N <= maxCnt and N = weighted
     * time-averaged freelist size (and only if the current freelist size is greater than N).
     */
    int popLRU(int destination, int bufType, int maxCnt, void **array, bool forced = false) {
        int result = 0;
        /* we don't do anything if we haven't been calculating stats and we're not forced... */
        if (!calcStats && !forced)
            return result;
        if (usethreads())
            lock.lock();
        struct peerInfo *info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        if (info) {
            int addrsToPop;
            if (calcStats) {
                /* update statistics */
                double now = dclock();
                double a0 = (now - info->time_lastupdated[bufType]) * info->lists[bufType].size();
                double a1 = (info->time_lastupdated[bufType] - info->time_started) *
                    info->time_avg_freelist_size[bufType];
                info->time_avg_freelist_size[bufType] = (now == info->time_started) ? 0 :
                    (a0 + a1)/(now - info->time_started);
                info->time_lastupdated[bufType] = now;
            }
            /* how much do we pop from the freelists? */
            if (forced) {
                addrsToPop = (info->lists[bufType].size() < maxCnt) ? info->lists[bufType].size() :
                    maxCnt;
            }
            else {
                addrsToPop = (info->lists[bufType].size() > (int)info->time_avg_freelist_size[bufType]) ?
                    (int)info->time_avg_freelist_size[bufType] : 0;
                addrsToPop = (addrsToPop > maxCnt) ? maxCnt : addrsToPop;
            }
            if (addrsToPop > 0) {
                result = info->lists[bufType].popLRU(array, addrsToPop);
            }
        }
        if (usethreads())
            lock.unlock();
        return result;
    }

    //! returns the current freelist sizes for all of the buffer types for a given peer
    /*! \param destination the global process id of the destination/peer process
     * \param counts the array of integers which must be NUMBER_BUFTYPES or larger to contain the sizes
     */
    void addrCounts(int destination, int *counts) {
        struct peerInfo *info;

        if (usethreads())
            lock.lock();

        info = tracker[destination & mask];
        /* find the right peer info structure */
        while (info && (info->destination != destination)) { info = info->next; }
        for (int i = 0; i < NUMBER_BUFTYPES; i++) {
            if (info)
                counts[i] = info->lists[i].size();
            else
                counts[i] = 0;
        }
        if (usethreads())
            lock.unlock();
    }

    //! prints the current state of this peerMemoryTracker (for debugging purposes)
    void dump(FILE *fp) {
        if (usethreads())
            lock.lock();
        fprintf(fp, "peerMemoryTracker at %p, usethreads() = %s:\n",
                this, usethreads() ? "true" : "false");
        fprintf(fp, "\ttracker = %p, slots = %ld, mask = 0x%lx, calcStats = %s\n",
                tracker, slots, mask, calcStats ? "true" : "false");
        for (int i = 0; i < slots; i++) {
            struct peerInfo *info = tracker[i];
            if (info) {
                fprintf(fp, "Slot %d:\n", i);
                while (info) {
                    fprintf(fp, "\tpeerInfo at %p, next = %p\n",
                            info, info->next);
                    fprintf(fp, "\t\tdestination = %d, time_started = %f\n",
                            info->destination, info->time_started);
                    for (int j = 0; j < NUMBER_BUFTYPES; j++) {
                        fprintf(fp, "\t\ttime_lastupdated[%d] = %f\n", j,
                                info->time_lastupdated[j]);
                    }
                    for (int j = 0; j < NUMBER_BUFTYPES; j++) {
                        fprintf(fp, "\t\ttime_avg_freelist_size[%d] = %f\n", j,
                                info->time_avg_freelist_size[j]);
                    }
                    for (int j = 0; j < NUMBER_BUFTYPES; j++) {
                        fprintf(fp, "\t\tlists[%d] at %p:\n", j,
                                &(info->lists[j]));
                        info->lists[j].dump(fp, "\t\t\t");
                    }
                    info = info->next;
                }
            }
        }
        fflush(fp);
        if (usethreads())
            lock.unlock();
    }
};

#endif
