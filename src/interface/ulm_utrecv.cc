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



#include "internal/log.h"
#include "internal/options.h"
#include "ulm/ulm.h"
#include "queue/globals.h"
#include "queue/globals.h"

/*!
 * Non-blocking receive of a multicast message
 *
 * \param comm		Communicator
 * \param tag		Tag to match
 * \param data		Data buffer
 * \param length	Size of data buffer in bytes
 * \param request	ULM request handle
 * \return		ULM return code
 */
extern "C" int ulm_utrecv(int comm, int tag, void *data,
			  size_t length, ULMType_t *dtype,
			  ULMRequestHandle_t *request)
{
    int errorCode = ULM_SUCCESS;
    Communicator *commPtr = communicators[comm];
    RequestDesc_t *tmpReq;

    if (OPT_CHECK_API_ARGS) {
	// check to see if this is a valid group
	if (commPtr == 0) {
	    ulm_err(("Error: ulm_utrecv: bad Group index %d\n", comm));
	    return ULM_ERR_COMM;
	}
    }

    // create temporary request object and initialize request
    errorCode = commPtr->utrecv_init(tag, data, length, dtype, tmpReq);
    if (errorCode != ULM_SUCCESS)
	return errorCode;

    // start collective receive
    errorCode = commPtr->utrecv_start(tmpReq);
    if (errorCode == ULM_ERR_OUT_OF_RESOURCE || errorCode == ULM_ERR_FATAL) {
	// free temporary request object
	ulm_request_free((ULMRequestHandle_t *) (&tmpReq));
	return errorCode;
    }

    tmpReq->status = ULM_STATUS_INCOMPLETE;
    *request = (ULMRequestHandle_t) tmpReq;

    return errorCode;
}
