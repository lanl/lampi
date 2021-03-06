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


#ifndef DARWIN_POWERPC_ATOMIC_H_INCLUDED
#define DARWIN_POWERPC_ATOMIC_H_INCLUDED

/*
 * The following atomic operations were adapted from the examples provided in the
 * PowerPC programming manual available at 
 * http://www-3.ibm.com/chips/techlib/techlib.nsf/techdocs/852569B20050FF778525699600719DF2
 */

#ifdef __cplusplus
extern "C" {
#endif

#define mb()    __asm__ __volatile__("sync")
#define rmb()    __asm__ __volatile__("sync")
#define wmb()    __asm__ __volatile__("sync")
    
/*
 * Lock structure
 */

enum { LOCK_UNLOCKED = 0 };

typedef struct {
    union {
        volatile int lockData_m;
        char padding[4];
    } data;
} lockStructure_t;


/*
 * 64 bit integer
 */
typedef struct {
    lockStructure_t lock;
    volatile unsigned long long data;
} bigAtomicUnsignedInt;


/*
 *  Spin until I can get the lock
 */
inline static void spinlock(lockStructure_t *lockData)
{
	volatile int		*lockptr = &(lockData->data.lockData_m);
	
    __asm__ __volatile__(
		"mr		r6, %0\n"		/* save the address of the lock. */
		"li    	 r4,1\n"
		"1:\n"
		"lwarx   r5,0,r6\n"		/* Get current lock value. */
		"cmpwi   r5,0x0\n"		/* Is it unlocked. if not, keep checking. */
		"bne-    1b\n"
		"stwcx.  r4,0,r6\n"		/* Try to atomically set the lock */
		"bne-    1b\n"		
		"isync\n" 
        : : "r" (lockptr)
		: "memory", "r4", "r5", "r6");
}

/*
 * This routine tries once to obtain the lock
 */
inline static int spintrylock(lockStructure_t *lockData)
{
	volatile int		*lockptr = &(lockData->data.lockData_m);
    int 	gotLock = 0;

    __asm__ __volatile__(
		"mr		r6, %1\n"		/* save the address of the lock. */
		"li     r4,0x1\n"	
	"1:\n"
		"lwarx	r5,0,r6\n"		
		"cmpwi  r5,0x0\n"		/* Is it locked? */
		"bne-   2f\n"			/* Yes, return 0 */
		"stwcx. r4,0,r6\n"		/* Try to atomically set the lock */
		"bne-   1b\n"
		"addi	%0,0,1\n"
		"isync\n" 
		"b		3f\n"
	"2:	addi	%0,0,0x0\n"
	"3:"
        : "=&r" (gotLock) : "r" (lockptr)
		: "memory", "r4", "r5", "r6" );
				
	return gotLock;
}

/*
 * Clear the lock
 */
inline static void spinunlock(lockStructure_t *lockData)
{
    lockData->data.lockData_m = LOCK_UNLOCKED;
}


/*
 *  atomically add a constant to the input integer returning the
 *  previous value
 */
inline static int fetchNadd(volatile int *addr, int inc)
{
    int inputValue;

    __asm__ __volatile__(
	"mr	r5,%2\n"				/* Save the increment */
"1:\n"
	"lwarx	%0, 0, %1\n"			/* Grab the area value */
	"add	r6, %0, r5\n"			/* Add the value */
	"stwcx.	r6, 0, %1\n"			/* Try to save the new value */
	"bne-	1b\n"			/* Didn't get it, try again... */
	"isync\n" 
	: "=&r" (inputValue) : "r" (addr), "r" (inc) : 
	"memory", "r5", "r6");
	
	return inputValue;
}


inline static int fetchNset(volatile int *addr, int setValue)
{
    int inputValue;

    __asm__ __volatile__(
	"mr	r5,%2\n"				/* Save the value to store */
"1:\n"
	"lwarx	%0, 0, %1\n"			/* Grab the area value */
	"stwcx.	r5, 0, %1\n"			/* Try to save the new value */
	"bne-	1b\n"			/* Didn't get it, try again... */
	"isync\n" 
	: "=&r" (inputValue) : "r" (addr), "r" (setValue) : 
	"memory", "r5");
	
	return inputValue;
}


inline static unsigned long long fetchNaddLong(bigAtomicUnsignedInt *addr,
                                               int inc)
{
    unsigned long long returnValue;

    spinlock(&(addr->lock));
    returnValue = addr->data;
    (addr->data) += inc;
    spinunlock(&(addr->lock));

    return returnValue;
}


inline static unsigned long long fetchNsetLong(bigAtomicUnsignedInt *addr,
                                               unsigned long long val)
{
    unsigned long long returnValue;

    spinlock(&(addr->lock));
    returnValue = addr->data;
    addr->data = val;
    spinunlock(&(addr->lock));

    return returnValue;
}


inline static unsigned long long fetchNaddLongNoLock(bigAtomicUnsignedInt *addr,
                                                     int inc)
{
    unsigned long long returnValue;

    returnValue = addr->data;
    addr->data += inc;

    return returnValue;
}

inline static void setBigAtomicUnsignedInt(bigAtomicUnsignedInt *addr,
                                           unsigned long long value)
{
    addr->data = value;
    addr->lock.data.lockData_m = LOCK_UNLOCKED;
}

#ifdef __cplusplus
}
#endif


#endif /* DARWIN_POWERPC_ATOMIC_H_INCLUDED */
