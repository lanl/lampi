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

#ifndef _ULM_INTERNAL_OPTIONS_H_
#define _ULM_INTERNAL_OPTIONS_H_

#include "internal/linkage.h"

CDECL_BEGIN

/* Constants to handle compile time options here */

/*
 * Check validity of API arguments
 */
#ifdef ENABLE_CHECK_API_ARGS
enum { OPT_CHECK_API_ARGS = 1 };
#else
enum { OPT_CHECK_API_ARGS = 0 };
#endif


/*
 * Shared Memory
 */

#ifdef ENABLE_SHARED_MEMORY
enum { OPT_SHARED_MEMORY = 1 };
#else
enum { OPT_SHARED_MEMORY = 0 };
#endif


/*
 * Multicast messaging
 */

#ifdef ENABLE_SHARED_MEMORY
enum { OPT_MCAST = 0 }; // Always zero for now
#else
enum { OPT_MCAST = 0 };
#endif


/*
 * Reliabitity
 */

#ifdef ENABLE_RELIABILITY
enum { OPT_RELIABILITY = 1 };
#else
enum { OPT_RELIABILITY = 0 };
#endif


/*
 * Debug output
 */

#ifdef ENABLE_DBG
enum { OPT_DBG = 1 };
#else
enum { OPT_DBG = 0 };
#endif


/*
 * Memory profiling
 */

#ifdef ENABLE_MEMPROFILE
enum { OPT_MEMPROFILE = 1 };
#else
enum { OPT_MEMPROFILE = 0 };
#endif


/*
 * Systems
 */

#ifdef ENABLE_BPROC
enum { OPT_BPROC = 1 };
#else
enum { OPT_BPROC = 0 };
#endif

#ifdef ENABLE_LSF
enum { OPT_LSF = 1 };
#else
enum { OPT_LSF = 0 };
#endif

#ifdef ENABLE_RMS
enum { OPT_RMS = 1 };
#else
enum { OPT_RMS = 0 };
#endif

#ifdef ENABLE_QSNET
enum { OPT_QSNET = 1 };
#else
enum { OPT_QSNET = 0 };
#endif

#ifdef ENABLE_GM
enum { OPT_GM = 1 };
#else
enum { OPT_GM = 0 };
#endif

CDECL_END

#endif /* !_ULM_INTERNAL_OPTIONS_H_ */
