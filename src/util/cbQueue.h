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

#ifndef _CBQUEUE
#define _CBQUEUE

#include <new>

#include "internal/malloc.h"
#include "internal/system.h"
#include "util/Lock.h"

/*
 *  This class uses a circular buffer for queue management
 */
#define CB_ERROR -1
typedef struct cbCtl_t {
    Locks lock;
    volatile unsigned int arrayIndex;
    volatile unsigned int numToClear;
} cbCtl;

template < class dataType, int MemFlags, int SharedMemFlag >
class cbQueue {
public:

    // default size
    enum {
        defaultSize = 512, lazyFreeFrequency = 500
    };

    // constructor - no operations to make sure the object can  be
    //   manipulated before any of it's data is touched.
    cbQueue() {
    } // initializer
    int init(int cbSize, int queueMemLocalityIndex,
             int headLockLocalityIndex,
             int tailLockLocalityIndex, dataType notInUseMarker,
             dataType Sig, dataType failSig) {
        int errorCode = ULM_SUCCESS;

        // set buffer size
        Size = cbSize;
        sigSlot = Sig;
        sigSlotFail = failSig;
        mask = (cbSize - 1);
        // allocate buffer space
        size_t lenToAllocate = sizeof(dataType);
        lenToAllocate *= cbSize;

        if (MemFlags == SharedMemFlag) {
            queue = (dataType *) PerProcSharedMemoryPools.getMemorySegment
                (lenToAllocate, CACHE_ALIGNMENT,
                 queueMemLocalityIndex);
        } else {
            queue = (dataType *) ulm_malloc(lenToAllocate);
        }
        if (!queue) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        // initialize queue
        unusedMarker = notInUseMarker;
        for (int i = 0; i < cbSize; i++) {
            queue[i] = unusedMarker;
        }

        // allocate head control structure
        if (MemFlags == SharedMemFlag) {
            head = (cbCtl *) PerProcSharedMemoryPools.getMemorySegment
                (sizeof(cbCtl), CACHE_ALIGNMENT,
                 headLockLocalityIndex);
        } else {
            head = (cbCtl *) ulm_malloc(sizeof(cbCtl));
        }
        if (!head) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        // run constructor
        new(&(head->lock)) Locks();
        head->arrayIndex = 0;

        // allocate tail control structure
        if (MemFlags == SharedMemFlag) {
            tail = (cbCtl *) PerProcSharedMemoryPools.getMemorySegment
                (sizeof(cbCtl), CACHE_ALIGNMENT,
                 tailLockLocalityIndex);
        } else {
            tail = (cbCtl *) ulm_malloc(sizeof(cbCtl));
        }
        if (!tail) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        // run constructor
        new(&(tail->lock)) Locks();
        tail->arrayIndex = 0;
        tail->numToClear = 0;

        return errorCode;
    }


    int writeToSlot(int slot, dataType * data) {
        int wroteToSlot = CB_ERROR;
        if (queue[slot] == sigSlot) {
            queue[slot] = *data;
            wroteToSlot = slot;
            return wroteToSlot;
        } else {
            return wroteToSlot;
        }
    }
    // write to the head
    //  return type - true, data written to queue
    //                false, data not written to queue
    int writeToHead(dataType * data) {
        int slot = CB_ERROR;
        head->lock.lock();
        int index = head->arrayIndex;
        if (queue[index] == unusedMarker) {
            slot = index;
            queue[slot] = *data;
            (head->arrayIndex)++;
            (head->arrayIndex) &= mask;
        }
        head->lock.unlock();
        return slot;
    }                           // returns the values of a slot

    int writeToHeadNoLock(dataType * data) {
        int slot = CB_ERROR;
        int index = head->arrayIndex;
        if (queue[index] == unusedMarker) {
            slot = index;
            queue[slot] = *data;
            (head->arrayIndex)++;
            (head->arrayIndex) &= mask;
        }
        return slot;
    }                           // returns the values of a slot

    // that has been reserved in the fifo
    // if no slot is available return -1
    // if inc = true; then the counter 
    // head of the list is incremented
    // otherwise we only increment 
    // fifo when we write to it.... 
    //  
    int getSlot() {
        int retVal = CB_ERROR;
        int index;
        head->lock.lock();
        index = head->arrayIndex;
        if (queue[index] == unusedMarker) {
            queue[index] = sigSlot;
            retVal = index;
            (head->arrayIndex)++;
            (head->arrayIndex) &= mask;
        }
        head->lock.unlock();
        return retVal;
    }

    int getSlotNoLock() {
        int retVal = CB_ERROR;
        int index;
        index = head->arrayIndex;
        if (queue[index] == unusedMarker) {
            queue[index] = sigSlot;
            retVal = index;
            (head->arrayIndex)++;
            (head->arrayIndex) &= mask;
        }
        return retVal;
    }

    // read from tail
    //  return type - true, data read from queue
    //                false, data not read from queue
    int readFromTail(dataType * data) {
        int readFromTail = CB_ERROR;
        int index = 0;
        int clearIndex;
        int i;

        // before we can readFromTail we have to 
        // make sure that that the "index" we are reading from
        // is not unmarked or sigSlot.  The data should
        // have been put into the fifo before

        if ((queue[tail->arrayIndex] == unusedMarker) ||
            (queue[tail->arrayIndex] == sigSlot))
            return CB_ERROR;

        // lock tail - for thread safety
        tail->lock.lock();

        /* make sure that there is still data to read */
        if ((queue[tail->arrayIndex] == unusedMarker) ||
            (queue[tail->arrayIndex] == sigSlot)) {
            tail->lock.unlock();
            return CB_ERROR;
        }

        index = tail->arrayIndex;
        *data = queue[index];
        tail->numToClear++;

        if (tail->numToClear == lazyFreeFrequency) {
            clearIndex = index - lazyFreeFrequency + 1;
            clearIndex &= mask;
            for (i = 0; i < lazyFreeFrequency; i++) {
                queue[clearIndex] = unusedMarker;
                clearIndex++;
                clearIndex &= mask;
            }
            tail->numToClear = 0;
        }
        readFromTail = tail->arrayIndex;
        (tail->arrayIndex)++;
        (tail->arrayIndex) &= mask;

        // unlock head
        tail->lock.unlock();

        return readFromTail;
    }

    int readFromTailNoLock(dataType * data) {
        int readFromTail = CB_ERROR;
        int index = 0;
        int clearIndex;
        int i;

        // before we can readFromTail we have to 
        // make sure that that the "index" we are reading from
        // is not unmarked or sigSlot.  The data should
        // have been put into the fifo before

        if ((queue[tail->arrayIndex] == unusedMarker) ||
            (queue[tail->arrayIndex] == sigSlot))
            return CB_ERROR;

        // At this point we are assming that
        // the data in the slot is legit.

        index = tail->arrayIndex;
        *data = queue[index];
        tail->numToClear++;

        if (tail->numToClear == lazyFreeFrequency) {
            clearIndex = index - lazyFreeFrequency + 1;
            clearIndex &= mask;
            for (i = 0; i < lazyFreeFrequency; i++) {
                queue[clearIndex] = unusedMarker;
                clearIndex++;
                clearIndex &= mask;
            }
            tail->numToClear = 0;
        }
        readFromTail = tail->arrayIndex;
        (tail->arrayIndex)++;
        (tail->arrayIndex) &= mask;

        return readFromTail;
    }

    // query for data
    bool thereAreElementsToRead() {
        return queue[tail->arrayIndex] != unusedMarker;
    }
    int tailArrayIndex() {
        return tail->arrayIndex;
    }
    int headArrayIndex() {
        return head->arrayIndex;
    }
    int Size;

    // head of queue - where next entry will be written
    cbCtl *head;

    // tail of queue - next element to read
    cbCtl *tail;

    // mask - to handle wrap around
    unsigned int mask;

    // marker for free element
    dataType unusedMarker;

    // marker to reserve slot
    dataType sigSlot;

    // when we fail to get a sig slot
    dataType sigSlotFail;

    // circular buffer
    dataType *queue;
};

#endif				/* !_CBQUEUE */
