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

/*
 *  Start processing the request - either send or receive.
 *
 *  \param request      ULM request handle
 *  \return             ULM return code
 */
extern "C" int ulm_start(ULMRequestHandle_t *request)
{
    RequestDesc_t *tmpRequest = (RequestDesc_t *) (*request);
    int errorCode;
    int comm = tmpRequest->ctx_m;
    Communicator *commPtr = communicators[comm];

    if (tmpRequest->requestType == REQUEST_TYPE_SEND) {
        /* send */
	    BaseSendDesc_t *SendDescriptor = (BaseSendDesc_t *) tmpRequest;
	/* for persistant send, may need to reset request */
	if ( tmpRequest->persistent ) {
		/* check to see if the SendDescriptor can be reused,
		 *   or if we need to allocate a new one
		 */
		int reuseDesc=0;
		if( (SendDescriptor->numfrags <= SendDescriptor->NumAcked) && 
				SendDescriptor->messageDone == REQUEST_COMPLETE
				) {
			reuseDesc=1;
		}
		if( !reuseDesc ) {
			/* allocate new descriptor */
			int dest=SendDescriptor->posted_m.proc.destination_m;
			BaseSendDesc_t *tmpSendDesc = SendDescriptor;
			errorCode= communicators[comm]->
				pt2ptPathSelectionFunction((void **) 
						(&SendDescriptor),comm,dest);
			if (errorCode != ULM_SUCCESS) {
				return errorCode;
		    	}

			/* fill in new descriptor information */
			*SendDescriptor=*tmpSendDesc;

			/* reset the old descriptor */
			tmpSendDesc->persistent=false;

			/* wait/test can't be called on the descriptor
			 *   any more, since we change the handle that
			 *   is returned to the app
			 */
			tmpSendDesc->messageDone=REQUEST_RELEASED;

			/* reset local parameters */
			*request=&SendDescriptor;
			tmpRequest = (RequestDesc_t *) (*request);
		}
	}
        errorCode = commPtr->isend_start(&SendDescriptor);
        if (errorCode != ULM_SUCCESS) {
            return errorCode;
        }
    } else if (tmpRequest->requestType == REQUEST_TYPE_RECV) {
        // receive
        errorCode = commPtr->irecv_start(request);
        if (errorCode != ULM_SUCCESS) {
            return errorCode;
        }
    } else {
        ulm_err(("Error: ulm_start: Unrecognized message request %d\n",
                 tmpRequest->requestType));
        errorCode = ULM_ERR_REQUEST;
    }

    return errorCode;
}
