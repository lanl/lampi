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



#ifndef _CLIENTTYPES
#define _CLIENTTYPES

#include "util/DblLinkList.h"
#include "internal/constants.h"
#include "ulm/ulm.h"

enum requestState {
	REQUEST_INCOMPLETE=0, 
	REQUEST_COMPLETE, 
	REQUEST_RELEASED 
};

//
// structure to define a request - either send or receive
//
typedef struct {
    size_t length_m;		// length of request
    union {
	int source_m;		// on a recv this is the source process
	int destination_m;	// on send this is the destination process
    } proc;
    int UserTag_m;		// user tag
} msgRequest;

//
// structure that defines both request and response to request
//
struct RequestDesc_t : public Links_t {

    ULMType_t *datatype;	// datatype (NULL => contiguous)

    int ctx_m;		// communicator ID
    int messageType;		// message type - point-to-point or collective
    int requestType;		// request type - send or receive
    int status;			// indicates request status

    bool persistent;		// persistence flag
    bool freeCalled;        // true when ulm_request_free is called before recv. request complete
    volatile int messageDone;	// message completion flag

    // default constructor
    RequestDesc_t() {}

    // constructor
    RequestDesc_t(int poolIndex) {
        WhichQueue = REQUESTFREELIST;
    }

    virtual void shallowCopyTo(RequestDesc_t *request)
    {
        /* Copies field values of current object to passed request.  This is mainly
           used for persistent requests where the underlying send descriptor could
           change (refer to ulm_start() logic).
        */
        request->datatype = datatype;
        request->ctx_m = ctx_m;
        request->messageType = messageType;
        request->requestType = requestType;
        request->persistent = persistent;
    }
};

#endif				/* _CLIENTTYPES */
