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
#include "ulm/constants.h"
#include "internal/log.h"
#include "queue/globals.h"

/*!
 * send multiple messages at once - this routine sets an upper limit
 *   on the actual number of messages to be sent.
 *
 * \param buf           Array of send buffers
 * \param size          number of dtype elements
 * \param dtype         Datatype descriptors
 * \param source        ProcIDs of source process
 * \param tag           Tags for matching
 * \param comm          IDs of communicator
 * \param numToReceive  Number of receives to post and complets
 * \param status        Array of status objects
 * \return              ULM return code
 */
extern "C" int ulm_send_vec(void **buf, size_t *size, ULMType_t **dtype,
                            int *destinationProc, int *tag, int *comm,
                            int numToSend,ULMStatus_t *status)
{
    ULMRequestHandle_t request[MAX_SENDS_TO_POST];
    int tmp[MAX_SENDS_TO_POST];
    int errorCode=ULM_SUCCESS;
    int numDone=0;
    int numIncomplete=0;
    int numPosted=0;

    /* initialize the status objects */
    for (int st=0 ; st < numToSend ; st++ ) {
        status[st].state=ULM_STATUS_INCOMPLETE;
    }

    /* initialize request objects */
    int maxSends=numToSend;
    if( numToSend > MAX_SENDS_TO_POST )
        maxSends=MAX_SENDS_TO_POST;
    for (int req=0 ; req < maxSends ; req++ ) {
        request[req]=ULM_REQUEST_NULL;
        tmp[req]=-1;
    }

    /* loop until all receives have completed */
    while( numDone < numToSend ) {
        /* check to see if can post new receive */
        if( (numIncomplete < MAX_SENDS_TO_POST ) &&
            ( numPosted < numToSend) ) {
            /* find free request object */
            int freeRequest=-1;
            for( int req=0 ; req < maxSends ; req++ ) {
                if( request[req]==ULM_REQUEST_NULL ){
                    freeRequest=req;
                    tmp[freeRequest]=numPosted;
                }
            }
            errorCode=ulm_isend(buf[numPosted], size[numPosted], dtype[numPosted],
                                destinationProc[numPosted], tag[numPosted], comm[numPosted],
                                request+freeRequest,ULM_SEND_STANDARD);
            if( errorCode != ULM_SUCCESS )
                return errorCode;
            /* update number of receives posted */
            numPosted++;numIncomplete++;
        }

        /* check to see if any receives have completed */
        for( int req=0 ; req < maxSends ; req++ ) {
            /* test if receive status is incomplet */
            if( request[req]!=ULM_REQUEST_NULL ) {
                int index=tmp[req];
                int completed;
                errorCode=ulm_test(request+req,&completed,status+index);
                if ((errorCode == ULM_ERR_OUT_OF_RESOURCE)
                    || (errorCode == ULM_ERR_FATAL)) {
                    return errorCode;
                }
                if( completed ) {
                    numIncomplete--;
                    numDone++;
                }
            }
        } /* end req loop */

    } /* end while( numDone < numToSend ) loop */

    return errorCode;
}
