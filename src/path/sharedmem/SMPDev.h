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





#ifndef _SMPDEV
#define _SMPDEV

#include "include/internal/mmap_params.h"
#include "mem/ULMMemoryBuckets.h"
#include "mem/ULMPool.h"
#include "mem/ULMMallocSharedLocal.h"
#include "internal/system.h"		// for SMPPAGESIZE

#define Log2ChunkSize 16
#define NumSMPMemoryBuckets 3
#define MaxStackElements 32768
#define MemProt PROT_READ|PROT_WRITE
#define MemFlags MAP_SHARED
#define ZeroAllocationBase false
#define PAGESPERDEV 16384 

class SMPDevDefaults {
public:

#ifdef __sgi
#include "os/IRIX/SMPDevParams.h"
#endif

#ifdef __osf__
#include "os/TRU64/SMPDevParams.h"
#endif

#ifdef __APPLE__
# include "os/DARWIN/powerpc/SMPDevParams.h"
#endif

#if defined(__linux__) && defined(__i386)
#include "os/LINUX/i686/SMPDevParams.h"
#endif

#if defined(__linux__) && defined(__alpha)
#include "os/LINUX/alpha/SMPDevParams.h"
#endif

#if defined(__linux__) && defined(__ia64)
#include "os/LINUX/ia64/SMPDevParams.h"
#endif
};

/*
 * logical device describing the SMP shared memory pools.
 */

typedef class ULMMallocSharedLocal<SMPDevDefaults::LogBase2MaxPoolSize, 
                                   SMPDevDefaults::LogBase2ChunkSize, 
                                   SMPDevDefaults::NumMemoryBuckets, 
                                   SMPDevDefaults::LogBase2PageSize, 
                                   SMPDevDefaults::MaxStkElements, 
                                   SMPDevDefaults::MProt, 
                                   SMPDevDefaults::MFlags, 
                                   SMPDevDefaults::ZeroAllocBase> smpDev;

template <long SizeOfClass, long SizeWithNoPadding>
class SMPSharedMem_log_dev : public smpDev
{
public:
    SMPSharedMem_log_dev(ULMMemoryPool* sp = 0) : smpDev(sp) {}
private:
    char padding[SizeOfClass-SizeWithNoPadding];
};

#ifdef __sgi
typedef SMPSharedMem_log_dev<57*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#ifdef __osf__
typedef SMPSharedMem_log_dev<110*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#ifdef __APPLE__
typedef SMPSharedMem_log_dev<110*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#if defined(__linux__) && defined(__i386)
typedef SMPSharedMem_log_dev<440*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#if defined(__linux__) && defined(__alpha)
typedef SMPSharedMem_log_dev<110*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#if defined(__linux__) && defined(__ia64)
typedef SMPSharedMem_log_dev<110*SMPPAGESIZE, sizeof(smpDev)>SMPSharedMem_logical_dev_t;
#endif

#undef Log2ChunkSize
#undef MaxStackElements
#undef MemProt
#undef MemFlags
#undef ZeroAllocationBase

#endif /* _SMPDEV */
