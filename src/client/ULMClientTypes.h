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

#define MESSAGE_INCOMPLETE false
#define MESSAGE_COMPLETE   true

//
// structure to define a request - either send or receive
//
typedef struct {
    size_t length_m;		// length of request
    size_t lengthProcessed_m;	// length actually processed
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

    // Locks Lock;		// object lock

    unsigned long long sequenceNumber_m;
    // sequence number (assigned on initiating side)

    void *pointerToData;	// pointer to buffer being sent/recvd;
                                // may be contiguous or not as
                                // determined by the datatype

    void *requestDesc;		// message descriptor structure; used
                                // only to communicate between
                                // send/recv_init and send/recv_start

    ULMType_t *datatype;	// datatype (NULL => contiguous)

    void *devContext;		// device specific context
    void *devBuffer;		// device specific system buffer

    void *appBufferPointer;	// set to point to the original buffer (which
                                // can be different from pointerToData -- MPI_Bsend)
    ULMType_t *bsendDtypeType;	// datatype of original bsend data
    size_t bsendBufferSize;	// the size of the buffer needed to pack the bsend data

    int datatypeCount;		// datatype element count
    int bsendDtypeCount;	// datatype element count of original bsend data
    int ctx_m;		// communicator ID
    int messageType;		// message type - point-to-point or collective
    int requestType;		// request type - send or receive
    int sendType;		// send type - normal, buffered, synchronous, or ready
    int status;			// indicates request status

    bool persistent;		// persistence flag
    bool freeCalled;        // true when ulm_request_free is called before recv. request complete
    volatile bool messageDone;	// message completion flag

    msgRequest posted_m;	// set by send/recv_init - never modified afterwards
    msgRequest reslts_m;	// values set as for each satisfied request
    /* debug */
    double t0;
    double t1;
    double t2;
    /* end debug */

    // default constructor
    RequestDesc_t() {}

    // constructor
    RequestDesc_t(int poolIndex) {
        WhichQueue = REQUESTFREELIST;
    }
};

#endif				/* _CLIENTTYPES */
