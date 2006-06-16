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



#ifndef LINUX_I686_ATOMIC_H_INCLUDED
#define LINUX_I686_ATOMIC_H_INCLUDED

/*
 * Lock structure
 */

enum { LOCK_UNLOCKED = 1 };

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
#ifdef __INTEL_COMPILER

#ifdef __cplusplus
extern "C"
{
#endif

    void spinlock(lockStructure_t *lockData);
    int spintrylock(lockStructure_t *lockData);
    int fetchNadd(volatile int *addr, int inc);
    int fetchNset(volatile int *addr, int setValue);

#ifdef __cplusplus
}
#endif


#else
*/

/*
 *  Spin until I can get the lock
 */
inline static void spinlock(lockStructure_t *lockData)
{
    __asm__ __volatile__(
        "cmp $1, %0\n"
        "jl 2f\n"
        "\n1: "
        "lock ; decl %0\n"
        "jz 3f\n"
        "2:\n"
        "cmp $1, %0\n"
        "jl 2b\n"
        "jmp 1b\n"
        "3:\n"
        : "=m" (lockData->data.lockData_m) :  : "memory");
}

/*
 * This routine tries once to obtain the lock
 */
inline static int spintrylock(lockStructure_t *lockData)
{
    int gotLock;

    __asm__ __volatile__(
        "mov %2, %1\n"
        "cmp $1, %0\n"
        "jl 1f\n"
        "lock ; decl %0\n"
        "js 1f\n"
        "mov $1, %1\n"
        "jmp 2f\n"
        "1:\n"
        "mov $0, %1\n"
        "2:"
        : "=m" (lockData->data.lockData_m),
#ifdef __INTEL_COMPILER
        "=&r" (gotLock) : "r" (0) : "memory");
#else
        "=r" (gotLock) : "r" (0) : "memory");
#endif

    return gotLock;
}


/*
 *  atomically add a constant to the input integer returning the
 *  previous value
 */
inline static int fetchNadd(volatile int *addr, int inc)
{
    int inputValue;
    __asm__ __volatile__(
	"       mov %2, %1\n" \
	"lock ; xadd %1, %0\n"
#ifdef __INTEL_COMPILER
	: "=m" (*addr), "=&r" (inputValue) : "r" (inc) : "memory");
#else
    : "=m" (*addr), "=r" (inputValue) : "r" (inc) : "memory");
#endif

    return (inputValue);
}


inline static int fetchNset(volatile int *addr, int setValue)
{
    int inputValue;

    __asm__ __volatile__(
	"       mov %2, %1\n" \
	"lock ; xchg %1, %0\n"
#ifdef __INTEL_COMPILER
	: "=m" (*addr), "=&r" (inputValue) : "r" (setValue) : "memory");
#else
    : "=m" (*addr), "=r" (inputValue) : "r" (setValue) : "memory");
#endif

    return (inputValue);
}

//#endif		/* __INTEL_COMPILER */


/*
 * Clear the lock
 */
inline static void spinunlock(lockStructure_t *lockData)
{
    __asm__ __volatile__("": : :"memory");
    lockData->data.lockData_m = 1;
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

#define mb __asm__ __volatile__("": : :"memory");
#define wmb __asm__ __volatile__("": : :"memory");
#define rmb __asm__ __volatile__("": : :"memory");


#endif /* LINUX_I686_ATOMIC_H_INCLUDED */
