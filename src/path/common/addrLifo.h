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

#ifndef _ADDRLIFO_H
#define _ADDRLIFO_H

#include <stdlib.h>
#include <stdio.h>

#include "internal/malloc.h"

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

#endif
