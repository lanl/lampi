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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "queue/SeqTrackingList.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/system.h"
#include "mem/FixedSharedMemPool.h"

extern FixedSharedMemPool SharedMemoryPools;

SeqTrackingList::SeqTrackingList(unsigned long startSize,
				 unsigned long startGrowBy,
				 unsigned long startShrinkBy,
				 bool shared)
{
    long i;

    headp = freep = hintp = 0;
    arraySize = startSize;
    if (shared) {
	growBy = 0;
	shrinkBy = 0;
	baseArray = (arraySize > 0) ?
	    (SeqRangeListElement *) SharedMemoryPools.getMemorySegment(
		arraySize * sizeof(SeqRangeListElement), CACHE_ALIGNMENT)
	    : (SeqRangeListElement *) 0;
    }
    else {
	growBy = startGrowBy;
	shrinkBy = startShrinkBy;
	baseArray = (arraySize > 0) ?
	    (SeqRangeListElement *) ulm_malloc(arraySize *
					   sizeof(SeqRangeListElement))
	    : (SeqRangeListElement *) 0;
    }
    if ((arraySize > 0) && (baseArray == 0)) {
	arraySize = 0;
    }
    listSize = arraySize;	// this will be decremented appropriately by freeRangeElement
    for (i = 0; i < arraySize; i++) {
	baseArray[i].next = baseArray[i].prev = 0;
	freeSeqRangeListElement(&baseArray[i]);
    }
    assert(listSize == 0);
}

//! Private member function to free a sequence range list element, and fix other list state
void SeqTrackingList::freeSeqRangeListElement(SeqTrackingList::SeqRangeListElement * ptr) {
    if (ptr->next != 0) {	// remove this element from headp's list
	(ptr->prev)->next = ptr->next;
	(ptr->next)->prev = ptr->prev;
	if (ptr == headp) {
	    if (ptr->next != ptr) {
		headp = ptr->next;
	    } else {
		headp = 0;
	    }}
    }
    // put in on freep's list, and fix other list state
    if (freep == 0) {
	freep = ptr;
	freep->next = freep->prev = freep;
    } else {	// append to the end of freep's circular list
	SeqRangeListElement *last = freep->prev;
	last->next = ptr;
	ptr->next = freep;
	freep->prev = ptr;
	ptr->prev = last;
    }

    // clear its entries, adjust headp's listSize, and remove any hints that point
    // to this list element (used in findNearestLowerSeqRangeListElement())
    ptr->lower = ptr->upper = 0;
    listSize--;
    if (hintp == ptr) {
	hintp = 0;
    }
    // shrink underlying storage array, if shrinkBy > 0, and
    // the difference between arraySize and listSize is greater than
    // or equal to shrinkBy
    if (shrinkBy && ((arraySize - listSize) > shrinkBy)) {
	if ((arraySize - shrinkBy) == 0) {
	    assert(listSize == 0);
	    freep = hintp = headp = 0;
	    ulm_free(baseArray);
	    baseArray = 0;
	    arraySize = 0;
	} else {
	    long i;
	    SeqRangeListElement *tmp = headp;
	    SeqRangeListElement *newArray =
		(SeqRangeListElement *)
		ulm_malloc((size_t)((arraySize - shrinkBy) *
				sizeof(SeqRangeListElement)));

	    if (newArray == 0) {
		return;
	    }	// forget about it for the moment...

	    freep = hintp = 0;

	    // rebuild list in newArray's memory
	    if (headp != 0) {
		headp = &newArray[0];
		for (i = 0; i < listSize; i++) {
		    newArray[i].next =
			(i ==
			 (listSize -
			  1)) ? &newArray[0] :
			&newArray[i + 1];
		    newArray[i].prev =
			(i ==
			 0) ?
			&newArray[listSize -
				  1] :
			&newArray[i - 1];
		    newArray[i].lower =
			tmp->lower;
		    newArray[i].upper =
			tmp->upper;
		    tmp = tmp->next;
		}
	    }
	    if (baseArray) {
		ulm_free(baseArray);
	    }
	    baseArray = newArray;
	    arraySize -= shrinkBy;

	    // rebuild free list if needed
	    for (i = listSize; i < arraySize; i++) {
		if (freep == 0) {
		    freep = &baseArray[i];
		    freep->next = freep->prev =
			freep;
		} else {
		    SeqRangeListElement *last =
			freep->prev;
		    last->next = freep->prev =
			&baseArray[i];
		    baseArray[i].next = freep;
		    baseArray[i].prev = last;
		    baseArray[i].lower =
			baseArray[i].upper = 0;
		}
	    }
	}
    }
}

//! Private member function to get an available sequence range list element
/*! \param prevp the new element is linked into the list after prevp,
 * if prevp == 0, then prepend the element to the whole list (as it exists)
 */
SeqTrackingList::SeqRangeListElement *SeqTrackingList::getSeqRangeListElement(SeqTrackingList::SeqRangeListElement *prevp) {
    SeqRangeListElement *result;
    long i;

    if (listSize + 1 > arraySize) {	// grow dynamic array
	if (growBy == 0) {
	    return 0;
	}
	SeqRangeListElement *newArray = (SeqRangeListElement *)ulm_malloc(
	    (size_t)((arraySize + growBy) * sizeof(SeqRangeListElement)));
	SeqRangeListElement *tmp = headp;
	if (newArray == NULL)
	{
	    return 0;
	}
	// let's rebuild the list in the new memory
	freep = 0;
	headp = (listSize > 0) ? &newArray[0] : 0;
	for (i = 0; i < listSize; i++)
	{
	    newArray[i].next = (i == (listSize - 1)) ? &newArray[0] :
		&newArray[i + 1];
	    newArray[i].prev = (i == 0) ?  &newArray[listSize - 1] :
		&newArray[i - 1];
	    newArray[i].lower = tmp->lower;
	    newArray[i].upper = tmp->upper;
	    // reset prevp to reflect the new array location
	    if (prevp == tmp)
	    {
		prevp = &newArray[i];
	    }
	    // reset the hintp to point at the new list element
	    if (hintp == tmp)
	    {
		hintp = &newArray[i];
	    }
	    tmp = tmp->next;
	}
	if (baseArray) {
	    ulm_free(baseArray);
	}
	baseArray = newArray;

	// let's add the new free elements...
	listSize += growBy;	// this will be decremented properly by freeRangeElement...
	for (i = 0; i < growBy; i++) {	// put these elements on the free queue
	    baseArray[arraySize + i].next =
		baseArray[arraySize + i].prev = 0;
	    freeSeqRangeListElement(&baseArray
				    [arraySize + i]);
	}
	// increment arraySize after freeSeqRangeListElement...
	arraySize += growBy;
    } // end - grow dynamic array

    // grab an element off of the free queue
    result = freep;
    (freep->prev)->next = freep->next;
    (freep->next)->prev = freep->prev;
    freep = freep->next;
    if (freep == result) {
	freep = 0;
    }

    if (prevp) {
        // append element after prevp
	    SeqRangeListElement *tmp = prevp->next;
	    prevp->next = result;
	    result->next = tmp;
	    tmp->prev = result;
	    result->prev = prevp;
    } else {
        // prepend element to existing list
        if (headp) {
	        result->next = headp;
	        result->prev = headp->prev;
	        headp->prev = result;
	        (result->prev)->next = result;
	        headp = result;
        } else {
	        assert(listSize == 0);
	        headp = result;
	        result->next = result->prev = result;
        }
    }

    // adjust listSize to reflect the new element's addition
    listSize++;
    return result;
}

bool SeqTrackingList::record(unsigned long long lower, unsigned long long upper)
{

    SeqRangeListElement *lnearest = findNearestLowerSeqRangeListElement(lower);
    SeqRangeListElement *unearest, *tmp;

    if (lower == 0) {
        // 0 should never actually be recorded
	    return true;
    } else if (lower > upper) {
        // never record an invalid range
        return false;
    }

    if (lnearest == 0) {	
        // we need to initialize the first element
	    lnearest = getSeqRangeListElement((SeqRangeListElement *) 0);
	    if (lnearest == 0) { return false; }
	    lnearest->lower = lower;
	    lnearest->upper = upper;
	    return true;
    }

    unearest = findNearestUpperSeqRangeListElement(lnearest, upper);

    if (lnearest == unearest) {
        // new range spans a single entry
        if (lnearest->lower > (upper + 1)) {
            // disjoint range before lnearest
            lnearest = getSeqRangeListElement((lnearest == headp) ? 0 : lnearest->prev);
	        if (lnearest == 0) { return false; }
            lnearest->lower = lower;
            lnearest->upper = upper;
        }
        else if ((lower - 1) > lnearest->upper) {
            // disjoint range after lnearest
            unearest = getSeqRangeListElement(lnearest);
	        if (unearest == 0) { return false; }
            unearest->lower = lower;
            unearest->upper = upper;
        }
        else {
            // partially or completely intersecting range
            if (upper > lnearest->upper) { lnearest->upper = upper; }
            if (lower < lnearest->lower) { lnearest->lower = lower; }
        }
    }
    else {
        // new range spans multiple entries
        if (lower <= (lnearest->upper + 1)) {
            // consolidate with lower range element!
            // reset lower range element upper sequence number
            lnearest->upper = upper;
            // reset lower range element lower seq. #
            if (lower < lnearest->lower)
                lnearest->lower = lower;
            // free overlapped range entries
            tmp = lnearest->next;
            while (tmp != unearest) {
                SeqRangeListElement *next = tmp->next;
                freeSeqRangeListElement(tmp);
                tmp = next;
            }
            // consolidate with upper range element, if appropriate
            if ((upper + 1) >= unearest->lower) {
                lnearest->upper = (upper > unearest->upper) ? upper : unearest->upper;
                freeSeqRangeListElement(unearest);
            }
        } else if ((upper + 1) < unearest->lower) {
            // disjoint range!
            if (lnearest->next == unearest) {
                // add new range element
                tmp = getSeqRangeListElement(lnearest);
                if (tmp == 0) { return false; }
                tmp->lower = lower;
                tmp->upper = upper;
            } else {
                // use next overlapped element for this range
                tmp = lnearest->next;
                tmp->lower = lower;
                tmp->upper = upper;
                tmp = tmp->next;
                // delete any other overlapped elements
                while (tmp != unearest) {
                    SeqRangeListElement *next = tmp->next;
                    freeSeqRangeListElement(tmp);
                    tmp = next;
                }
            }
        } else {
            // consolidate with upper range element!
            // reset upper range element lower sequence number
            unearest->lower = lower;
            // reset upper range element upper seq. #
            if (upper > unearest->upper)
                unearest->upper = upper;
            // free overlapped range entries
            tmp = lnearest->next;
            while (tmp != unearest) {
                SeqRangeListElement *next = tmp->next;
                freeSeqRangeListElement(tmp);
                tmp = next;
            }
        }
    }

#ifdef _DEBUGSEQTRACKINGLISTS
    if (!OK()) {
	ulm_dbg(("*** SeqTrackingList::record - recorded %lld - %lld\n", lower, upper));
	dump();
    }
#endif

    return true;
}

bool SeqTrackingList::erase(unsigned long long lower, unsigned long long upper)
{
    SeqRangeListElement *lnearest = findNearestLowerSeqRangeListElement(lower);
    SeqRangeListElement *unearest, *tmp;

    if (lnearest == 0) {
        // nothing to do!
	    return true;
    } else if ((lower == 0) || (lower > upper)) {
        // can't erase invalid range...
        return false;
    }

    unearest = findNearestUpperSeqRangeListElement(lnearest, upper);

    if (lnearest != unearest) {
        // free overlapped range entries, if any
        tmp = lnearest->next;
        while (tmp != unearest) {
            SeqRangeListElement *next = tmp->next;
            freeSeqRangeListElement(tmp);
            tmp = next;
        }
        // free range element or reset the appropriate range entries
        if (lower <= lnearest->lower) {
            freeSeqRangeListElement(lnearest);
        }
        else if (lower <= lnearest->upper) {
            lnearest->upper = (lower - 1);
            if (lnearest->upper < lnearest->lower)
                freeSeqRangeListElement(lnearest);
        }
        if (upper >= unearest->upper) {
            freeSeqRangeListElement(unearest);
        }
        else if (upper >= unearest->lower) {
            unearest->lower = (upper + 1);
            if (unearest->upper < unearest->lower)
                freeSeqRangeListElement(unearest);
        }
    } else {
        // reset the appropriate range entries
        if ((lnearest->lower <= upper) && (lower <= lnearest->upper)) {
            // partially or completely intersecting range
            if (lower < lnearest->lower) {
                lnearest->lower = (upper + 1);
            } else if (upper > lnearest->upper) {
                lnearest->upper = (lower - 1);
            } else {
                if (lnearest->upper > upper) {
                    tmp = getSeqRangeListElement(lnearest);
                    if (tmp == 0) { return false; }
                    tmp->upper = lnearest->upper;
                    tmp->lower = (upper + 1);
                }
                lnearest->upper = (lower - 1);
            }
            if (lnearest->upper < lnearest->lower)
                freeSeqRangeListElement(lnearest);
        }
    }

#ifdef _DEBUGSEQTRACKINGLISTS
    if (!OK()) {
	ulm_dbg(("*** SeqTrackingList::erase - erased %lld - %lld\n", lower, upper));
	dump();
    }
#endif

    return true;
}

//! returns whether or not a sequence number range is partially/completely recorded
SeqTrackingList::recorded_t SeqTrackingList::isRecorded(unsigned long long lower, unsigned long long upper)
{
    SeqRangeListElement *lnearest = findNearestLowerSeqRangeListElement(lower);
    SeqRangeListElement *unearest;

    if (lnearest == 0) {
	    return NO;
    }

    unearest = findNearestUpperSeqRangeListElement(lnearest, upper);

    if (lnearest != unearest) {
        if (lnearest->next !=  unearest)
            return PARTIAL;
        if ((lower > lnearest->upper) && (upper < unearest->lower)) 
            return NO;
        else 
            return PARTIAL;
    }
    else {
        if ((lnearest->lower <= upper) && (lower <= lnearest->upper)) {
            if ((lower >= lnearest->lower) && (upper <= lnearest->upper)) {
                return COMPLETE;
            }
            else
                return PARTIAL;
        } else
            return NO;
    }
}

//! returns whether or not seqnum range was recorded before attempting to record it and won't if partial/completely recorded already
SeqTrackingList::recorded_t SeqTrackingList::recordIfNotRecorded(unsigned long long lower, unsigned long long upper, bool *recordStatus)
{

    SeqRangeListElement *lnearest = findNearestLowerSeqRangeListElement(lower);
    SeqRangeListElement *unearest, *tmp;
    recorded_t alreadyRecorded = NO;

    *recordStatus = true;

    if ((lower == 0) || (lower > upper)) {
        // 0 should never actually be recorded
        // never record an invalid range
        *recordStatus = false;
	    return alreadyRecorded;
    }

    if (lnearest == 0) {	
        // we need to initialize the first element
	    lnearest = getSeqRangeListElement((SeqRangeListElement *) 0);
	    if (lnearest == 0) { *recordStatus = false; return alreadyRecorded; }
	    lnearest->lower = lower;
	    lnearest->upper = upper;
	    return alreadyRecorded;
    }

    unearest = findNearestUpperSeqRangeListElement(lnearest, upper);

    if (lnearest == unearest) {
        // new range spans a single entry
        if (lnearest->lower > (upper + 1)) {
            // disjoint range before lnearest
            lnearest = getSeqRangeListElement((lnearest == headp) ? 0 : lnearest->prev);
	        if (lnearest == 0) { *recordStatus = false; return alreadyRecorded; }
            lnearest->lower = lower;
            lnearest->upper = upper;
        }
        else if ((lower - 1) > lnearest->upper) {
            // disjoint range after lnearest
            unearest = getSeqRangeListElement(lnearest);
	        if (unearest == 0) { *recordStatus = false; return alreadyRecorded; }
            unearest->lower = lower;
            unearest->upper = upper;
        }
        else {
            // partially or completely intersecting or adjacent range
            if ((upper >= lnearest->lower) && (lower <= lnearest->upper)) {
                if ((lower >= lnearest->lower) && (upper <= lnearest->upper)) 
                    alreadyRecorded = COMPLETE;
                else
                    alreadyRecorded = PARTIAL;
                return alreadyRecorded;
            }
            if (upper > lnearest->upper) { lnearest->upper = upper; }
            if (lower < lnearest->lower) { lnearest->lower = lower; }
        }
    }
    else {
        // partially recorded with lower or upper or intermediate range elements!
        if ((lower <= lnearest->upper) || (upper >= unearest->lower) ||
            (lnearest->next != unearest)) {
            alreadyRecorded = PARTIAL;
            return alreadyRecorded;
        }
        // new range spans multiple entries
        if (lower <= (lnearest->upper + 1)) {
            // consolidate with lower range element!
            // reset lower range element upper sequence number
            lnearest->upper = upper;
            // consolidate with upper range element, if appropriate
            if ((upper + 1) >= unearest->lower) {
                lnearest->upper = unearest->upper;
                freeSeqRangeListElement(unearest);
            }
        } else if ((upper + 1) < unearest->lower) {
            // disjoint range!
            // add new range element
            tmp = getSeqRangeListElement(lnearest);
            if (tmp == 0) { *recordStatus = false; return alreadyRecorded; }
            tmp->lower = lower;
            tmp->upper = upper;
        } else {
            // consolidate with upper range element!
            // reset upper range element lower sequence number
            unearest->lower = lower;
        }
    }

#ifdef _DEBUGSEQTRACKINGLISTS
    if (!OK()) {
	ulm_dbg(("*** SeqTrackingList::recordIfNotRecorded - %lld - %lld, returns %d, set recordStatus to %s\n", lower, upper,
        alreadyRecorded, *recordStatus ? "true" : "false"));
	dump();
    }
#endif

    return alreadyRecorded;
}

#ifdef _DEBUGSEQTRACKINGLISTS
void SeqTrackingList::dump()
{
    long freecnt = 0;
    SeqRangeListElement *tmp;

    ulm_dbg(("baseArray range = 0x%lx - 0x%lx\n", baseArray, (&baseArray[arraySize] - 1)));
    ulm_dbg(("listSize = %ld\n", listSize));
    ulm_dbg(("arraySize = %ld\n", arraySize));
    ulm_dbg(("growBy = %ld\n", growBy));
    ulm_dbg(("shrinkBy = %ld\n", shrinkBy));
    ulm_dbg(("headp = 0x%lx\n", headp));
    ulm_dbg(("freep = 0x%lx\n", freep));
    ulm_dbg(("hintp = 0x%lx\n", hintp));
    if (freep != 0) {
	tmp = freep;
	do {
	    freecnt++;
	    tmp = tmp->next;
	} while (tmp != freep);
    }
    ulm_dbg(("number of list elements on freep's circular queue = %ld\n",
	     freecnt));
    if (headp != 0) {
	tmp = headp;
	do {
	    ulm_dbg(("[%lld - %lld]: ptr = 0x%lx, next = 0x%lx, prev = 0x%lx\n",
		     tmp->lower, tmp->upper, tmp, tmp->next,
		     tmp->prev));
	    tmp = tmp->next;
	} while (tmp != headp);
    }
}
#endif
