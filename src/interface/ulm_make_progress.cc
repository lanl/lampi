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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#define ENABLE_TIMESTAMP 0

#include "internal/profiler.h"
#include "internal/timestamp.h"
#include "path/common/path.h"
#include "path/common/pathContainer.h"
#include "queue/globals.h"
#include "ulm/ulm.h"

#if ENABLE_UDP
# include "path/udp/UDPNetwork.h"
#endif // UDP

#if ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"

#endif // SHARED_MEMORY

#if ENABLE_RELIABILITY
#include "util/DblLinkList.h"
#endif

/*!
 * ulm_make_progress()
 *
 *   make library progress
 *    - try and send frags
 *    - try and receive and match new frags
 *
 * \return		ULM return code
 */
extern "C" int ulm_make_progress(void)
{
    BasePath_t *pathArray[MAX_PATHS];
    double now;
    int pathCount;
    int rc = ULM_SUCCESS;

    TIMESTAMP(0);

    // check for completed sends
#if ENABLE_RELIABILITY
    now = dclock();
#else
    now = 0;
#endif
    CheckForAckedMessages(now);

    TIMESTAMP(1);

    // check if there are any fragments to be pushed along
    rc = push_frags_into_network(now);
    if (rc == ULM_ERR_OUT_OF_RESOURCE || rc == ULM_ERR_FATAL) {
        return rc;
    }

    TIMESTAMP(2);

    pathCount = pathContainer()->allPaths(pathArray, MAX_PATHS);
    for (int i = 0; i < pathCount; i++) {
        if (pathArray[i]->needsPush()) {
            if (!pathArray[i]->push(now, &rc)) {
                if (rc == ULM_ERR_OUT_OF_RESOURCE || rc == ULM_ERR_FATAL) {
                    return rc;
                }
            }
        }
        if (!pathArray[i]->receive(now, &rc)) {
            if (rc == ULM_ERR_OUT_OF_RESOURCE || rc == ULM_ERR_FATAL) {
                return rc;
            }
        }
    }

    TIMESTAMP(3);

    // send acks

    SendUnsentAcks();

    TIMESTAMP(4);

    TIMESTAMP_REPORT(1000);

    return rc;
}
