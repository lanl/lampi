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

#ifndef _LOCKS
#define _LOCKS

#include <assert.h>

#include "os/atomic.h"
#include "ulm/errors.h"

//!  This is a general lock class, with methods to aquire a
//!    lock, try and aquire a lock, and release a lock.
//!   lock() - will not return until the lock has been obtained
//!   trylock() - will return 1/0 if the lock was/was-not obtained
//!   unlock() - release the lock.
//!   init() - used to initialize the functions called by lock(),
//!            trylock(), and unlock(), and must be called before
//!            lock can be invoked.

class Locks
{
public:

    //! constructor
    Locks()
        {
            ctlData_m.data.lockData_m = LOCK_UNLOCKED;
            inited_m = 1;
        }

    //! deprecated this function over time
    int init()
        {
            ctlData_m.data.lockData_m = LOCK_UNLOCKED;
            inited_m = 1;
            return ULM_SUCCESS;
        }

    void lock()
        {
            assert(inited_m == 1);
            spinlock(&ctlData_m);
        }

    void unlock()
        {
            assert(inited_m == 1);
            spinunlock(&ctlData_m);
        }

    int trylock()
        {
            assert(inited_m == 1);
            return spintrylock(&ctlData_m);
        }

//private:

    lockStructure_t ctlData_m;
    int inited_m;
};


#endif /* !_LOCKS */

