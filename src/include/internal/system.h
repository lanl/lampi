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



#ifndef _ULM_INTERNAL_SYSTEM_H_
#define _ULM_INTERNAL_SYSTEM_H_

#include "internal/linkage.h"

CDECL_BEGIN

/* conditionally include system dependent stuff here */

#if defined (__mips)

#include "os/IRIX/ulm_os.h"

#elif defined (__linux__)

#include "os/LINUX/ulm_os.h"

#elif defined (__osf__)

#include "os/TRU64/ulm_os.h"

#elif defined (__APPLE__)

#include "os/DARWIN/ulm_os.h"

#elif defined (__CYGWIN__)

#include "os/CYGWIN/ulm_os.h"

#else
# error
#endif

CDECL_END

#ifndef MEMCOPY_FUNC
#include "util/Utility.h"
#define MEMCOPY_FUNC bcopy2
#endif

#ifndef SMPSecondFragPayload
#define SMPSecondFragPayload 32768
#endif /* SMPSecondFragPayload */

#ifndef SMPFirstFragPayload
#define SMPFirstFragPayload  3496
#endif /* SMPFirstFragPayload */

#ifndef UDP_MIN_RECVBUF
#define UDP_MIN_RECVBUF 8000000
#endif /* UDP_MIN_RECVBUF */

#ifndef UDP_MIN_SENDBUF
#define UDP_MIN_SENDBUF 8000000
#endif /* UDP_MIN_RECVBUF */

#ifndef MAX_RECVS_TO_POST
#define MAX_RECVS_TO_POST  (int)16
#endif /* MAX_RECVS_TO_POST */

#ifndef GATHER_TREE_ORDER
#define GATHER_TREE_ORDER (int)3
#endif /* GATHER_TREE_ORDER */

#ifndef MAX_SENDS_TO_POST
#define MAX_SENDS_TO_POST  (int)16
#endif /* MAX_SENDS_TO_POST */

#ifndef SCATTER_TREE_ORDER
#define SCATTER_TREE_ORDER (int)3
#endif /* GATHER_TREE_ORDER */

#endif /* !_ULM_INTERNAL_SYSTEM_H_ */
