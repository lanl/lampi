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



#include <stdio.h>
#include "internal/profiler.h"
#include "internal/options.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"

/*!
 * Post non-blocking receive
 *
 * \param buf           Buffer for received buf
 * \param size          Size of buffer in bytes if dtype is NULL,
 *                      else number of datatype descriptors
 * \param dtype         Datatype descriptor
 * \param source        ProcID of source process
 * \param tag           Tag for matching
 * \param comm          ID of communicator
 * \param request       ULM request handle
 * \return              ULM return code
 */
extern "C" int ulm_irecv(void *buf, size_t size, ULMType_t *dtype,
                         int sourceProc, int tag, int comm,
                         ULMRequestHandle_t *request)
{
    ULMRequestHandle_t tmpRequest;
    Communicator *commPtr;
    RequestDesc_t *libRequest;
    int persistent;
    int errorCode;

    commPtr = communicators[comm];

    // initialize recv
    persistent = 0;
    errorCode = ulm_irecv_init((void *) buf, size, dtype, sourceProc,
                               tag, comm, &tmpRequest, persistent);
    if (errorCode != ULM_SUCCESS) {
        return errorCode;
    }
    libRequest = (RequestDesc_t *) tmpRequest;

    // start the recv
    errorCode = commPtr->irecv_start(&tmpRequest);
    if (errorCode != ULM_SUCCESS) {
        ulm_request_free(&tmpRequest);
        return errorCode;
    }

    // update request object
    libRequest->status = ULM_STATUS_INCOMPLETE;
    *request = tmpRequest;

    return ULM_SUCCESS;
}
