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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"

/*!
 * Test posted request(s) for completion without blocking
 * ulm_test must be called to properly get status and
 * deallocate request objects
 *
 * \param requestArray  Array of requests to test for completion
 * \param numRequests   Number of requests in requestArray
 * \param completed     Flag to set if request is complete
 * \return              ULM return code
 */
extern "C" int ulm_testall(ULMRequest_t *requestArray, int numRequests,
                           int *completed)
{
    RequestDesc_t *tmpRequest;
    int i, numComplete = 0;
    int ReturnValue = ULM_SUCCESS;

    // initialize completion status to incomplete
    *completed = 0;

    // move data along
    ReturnValue = ulm_make_progress();
    if ((ReturnValue == ULM_ERR_OUT_OF_RESOURCE) ||
        (ReturnValue == ULM_ERR_FATAL) || (ReturnValue == ULM_ERROR)) {
        return ReturnValue;
    }

    for (i = 0; i < numRequests; i++) {

        /*
         * if request is NULL, follow MPI conventions and return success
         */
        if (*requestArray == NULL || *requestArray == ULM_REQUEST_NULL) {
            numComplete++;
            requestArray++;
            continue;
        }

        // get pointer to ULM request object
        tmpRequest = (RequestDesc_t *) (*requestArray);
        if (tmpRequest->status == ULM_STATUS_COMPLETE ||
            tmpRequest->status == ULM_STATUS_INACTIVE) {
            numComplete++;
            requestArray++;
            continue;
        }

        if (tmpRequest->messageDone == REQUEST_COMPLETE ) {
            numComplete++;
            requestArray++;
        }
    }                           /* end for loop */

    if (numComplete == numRequests) {
        *completed = 1;
    }

    return ReturnValue;
}
