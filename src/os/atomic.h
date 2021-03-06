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

#ifndef ATOMIC_H_INCLUDED
#define ATOMIC_H_INCLUDED

/*
 * Atomic functions
 */

#ifdef __alpha
#  ifdef __GNUC__
#    include "os/LINUX/alpha/atomic.h"
#  else
#    include "os/TRU64/atomic.h"
#  endif
#endif

#if defined (__linux__) && defined (__i386)
#include "os/LINUX/i686/atomic.h"
#endif

#if defined (__linux__) && defined(__x86_64__)
#include "os/LINUX/x86_64/atomic.h"
#endif

#ifdef __CYGWIN__
#include "os/CYGWIN/atomic.h"
#endif

#ifdef __ia64
#include "os/LINUX/ia64/atomic.h"
#endif

#ifdef __mips
#include "os/IRIX/atomic.h"
#endif

#ifdef __APPLE__
/* check if PowerPC 970 (G5) */
#ifdef __ppc_64__
#include "os/DARWIN/ppc_64/atomic.h"
#else
#include "os/DARWIN/powerpc/atomic.h"
#endif

#endif      /* __APPLE__ */

#ifndef mb
#define mb()
#endif

#ifndef rmb
#define rmb()
#endif

#ifndef wmb
#define wmb()
#endif

/*
 * macros
 */

#define ATOMIC_LOCK_INIT(LOCKPTR)  spinunlock(LOCKPTR)
#define ATOMIC_LOCK(LOCKPTR)       spinlock(LOCKPTR)
#define ATOMIC_UNLOCK(LOCKPTR)     spinunlock(LOCKPTR)
#define ATOMIC_TRYLOCK(LOCKPTR)    spintrylock(LOCKPTR)

#endif /* ATOMIC_H_INCLUDED */
