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



#ifndef _SEQTRACKINGLIST
#define _SEQTRACKINGLIST

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "internal/malloc.h"

class SeqTrackingList {

    struct SeqRangeListElement {
        SeqRangeListElement *next;
        SeqRangeListElement *prev;
        unsigned long long lower;
        unsigned long long upper;
    };

private:
    // state
    SeqRangeListElement * baseArray; //!< Storage array the list is based on
    long arraySize;		//!< Total number of elements capable of being stored in baseArray
    long listSize;		//!< Number of elements in list (listSize <= arraySize)
    long growBy;		//!< Number of elements to add when growing storage array
    long shrinkBy;		//!< Number of elements to free when shrinking storage array

    SeqRangeListElement *headp;	//!< Head of the circular double linked list
    SeqRangeListElement *freep;	//!< Head of a circular double linked list with free elements
    SeqRangeListElement *hintp;	//!< last list element found by findNearestSeqRangeListElement()

    //! Private member function to free a sequence range list element, and fix other list state
    void freeSeqRangeListElement(SeqRangeListElement * ptr);

    //! Private member function to get an available sequence range list element
    /*! \param prevp the new element is linked into the list after prevp,
     * if prevp == 0, then prepend the element to the whole list (as it exists)
     */
    SeqRangeListElement *getSeqRangeListElement(SeqRangeListElement *prevp);

    //! Private member function to quickly find the nearest-lower reference to a given sequence number
    /*! nearest-lower is defined as either the element that encompasses the sequence number,
     * or contains a recorded sequence range that is less than the sequence number (and
     * no other sequence is closer and still less than the sequence number).
     */
    SeqRangeListElement *findNearestLowerSeqRangeListElement(unsigned long long seqnum) {
        SeqRangeListElement *startp;
        SeqRangeListElement *tmp;

        if (listSize == 0) {
            return 0;
        }
        startp = (hintp == 0) ? headp : hintp;
        tmp = startp;

        do {
            if (seqnum >= tmp->lower) {
                if (tmp->next == headp) {
                    hintp = tmp;
                    return tmp;
                }
                else {
                    if (seqnum < (tmp->next)->lower) {
                        hintp = tmp;
                        return tmp;
                    } 
                    tmp = tmp->next;
                }
            } else {
                if (tmp == headp) {
                    hintp = tmp;
                    return tmp;
                }
                tmp = tmp->prev;
            }
        } while (tmp != startp);

        hintp = tmp;
        return tmp;
    }

    //! Private member function to quickly find the nearest-upper reference to a given sequence number
    /*! nearest-upper is defined as either the element that encompasses the sequence number,
     * or contains a recorded sequence range that is greater than the sequence number (and
     * no other sequence is closer and still greater than the sequence number).
     */
    SeqRangeListElement *findNearestUpperSeqRangeListElement(SeqRangeListElement *lower, unsigned long long seqnum) {
        SeqRangeListElement *startp;
        SeqRangeListElement *tmp;

        if (listSize == 0) {
            return 0;
        }
        startp = (lower == 0) ? headp : lower;
        tmp = startp;

        do {
            if (seqnum <= tmp->upper) {
                if (tmp == headp) {
                    return tmp;
                }
                else {
                    if (seqnum > (tmp->prev)->upper) {
                        return tmp;
                    } 
                    tmp = tmp->prev;
                }
            } else {
                if (tmp->next == headp) {
                    return tmp;
                }
                tmp = tmp->next;
            }
        } while (tmp != startp);

        return tmp;
    }

public:
    //! Constructor
    /*! \param startSize The initial size of the underlying storage array in terms of sequence ranges (def. 0)
     * \param startGrowBy The number of sequence range elements to grow storage when required (default 10)
     * \param startShrinkBy The number of sequence range elements to shrink storage when possible (default 20)
     * \param shared If true, then the grow and shrink by parameters are ignored and baseArray is in shared memory (default false)
     */
    SeqTrackingList(unsigned long startSize = 0,
                    unsigned long startGrowBy = 10,
                    unsigned long startShrinkBy = 20,
                    bool shared = false);

    //! Destructor
    ~SeqTrackingList() {
        if (baseArray != 0) {
            ulm_free(baseArray);
            baseArray = 0;
        }
    }

    enum recorded_t { NO, PARTIAL, COMPLETE };

    //methods
    
    //! Record a sequence number range in the SeqTrackingList
    bool record(unsigned long long lower, unsigned long long upper);
    //! Record a sequence number in the SeqTrackingList
    bool record(unsigned long long seqnum) {
        return record(seqnum, seqnum);
    }

    //! Record a sequence number range if it is not already (even partially) recorded
    recorded_t recordIfNotRecorded(unsigned long long lower, unsigned long long upper, bool *recordStatus);
    //! Record a sequence number if it is not already recorded
    bool recordIfNotRecorded(unsigned long long seqnum, bool *recordStatus) {
        recorded_t result = recordIfNotRecorded(seqnum, seqnum, recordStatus);
        return (result == COMPLETE) ? true : false;
    }

    //! Erase any record of a given sequence number range in the SeqTrackingList
    bool erase(unsigned long long lower, unsigned long long upper);
    //! Erase any record of a given sequence number in the SeqTrackingList
    bool erase(unsigned long long seqnum) {
        return erase(seqnum, seqnum);
    }

    //! Return the largest in-order sequence number (1..(2^64 - 1)) recorded in the SeqTrackingList
    unsigned long long largestInOrder() { if (headp != 0) {
        return (headp->lower == 1) ? headp->upper : 0;
    }
    return 0;
    }

    //! Return true if a given sequence number range has been recorded in the SeqTrackingList
    recorded_t isRecorded(unsigned long long lower, unsigned long long upper);
    //! Return true if a given sequence number has been recorded in the SeqTrackingList
    bool isRecorded(unsigned long long seqnum) {
        recorded_t result = isRecorded(seqnum, seqnum);
        return (result == COMPLETE) ? true : false;
    }

    //! Return size of underlying storage array in # of list elements
    long getArraySize() { return arraySize; }

#ifdef _DEBUGSEQTRACKINGLISTS
    //! Print the internal state of the SeqTrackingList to standard output
    void dump();

    //! check the list to see if it is well constructed...
    bool OK() {
        SeqRangeListElement *tmp = headp;
        SeqRangeListElement *aftertmp;
        bool result = true;

        if (listSize == 0) { return true; }

        do {
            aftertmp = tmp->next;
            if (tmp->lower > tmp->upper)
            {
                result = false;
                break;
            } else if (aftertmp->lower <= (tmp->upper + 1))
            {
                if (aftertmp != headp)
                {
                    result = false;
                    break;
                }
            }
            tmp = tmp->next;
        } while (tmp != headp);

        return result;
    }
#endif
};

#endif
