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



#ifndef _FETCHANDOP
#define _FETCHANDOP

//! structure defining data needed for fetch and add barrier with
//!   hardware supporting atomic variable access

typedef struct {
    int commSize;                    //! number of processes participating in the barrier
    //!  process private memory
    int hubIndex;                    //! index into the hub resource affinity list
    //!  process private memory
    long long releaseCnt;            //! value that fetchOpVar must have to "realese" processes
    //!  process private memory
    volatile long long *fetchOpVar;  //! pointer to atmoic variable - process shared memory

    volatile long long currentCounter;  //! counter used for storing fetch op data - used
    //!  to the compiler from optimizing out reading
    //!  the atomic variable
} barrierFetchOpData;


typedef struct {
    int inUse;                       //! element in use
    int commRoot;                    //! commroot and groupID form a unique idendifier for
    //!   process group using this data
    int contextID;

    int nAllocated;                  //! number of times this element has been allocated
    int nFreed;                      //! number of free's for this element

    barrierFetchOpData *barrierData; //! pointer to barrier data
} barrierFetchOpDataCtlData;

//! management stucture for barrierFetchOpData pool
typedef struct {

    int nPools ;   // number of pools, 1 per hub
    barrierFetchOpDataCtlData **pool;
    int *nElementsInPool;
    int *lastPoolUsed; // in shared memory
    Locks *Lock; // in shared memory
} atomicHWBarrierPool;

#endif /* !_FETCHANDOP */
