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

#include <assert.h>

#include "internal/options.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/path.h"
#include "queue/globals.h"

BasePath_t::~BasePath_t(){}

void BasePath_t::ReturnDesc(SendDesc_t *message, int poolIndex)
{
    // sanity check - list must be empty
#ifndef _DEBUGQUEUES
    assert(message->FragsToSend.size() == 0);
    assert(message->FragsToAck.size() == 0);
#else
    if (message->FragsToSend.size() != 0L) {
        ulm_exit((-1, "sharedmemPath::ReturnDesc: message %p "
                  "FragsToSend.size() %ld numfrags %d numsent %d "
                  "numacked %d list %d\n", message, 
		  message->FragsToSend.size(),
                  message->numfrags, message->NumSent, message->NumAcked,
                  message->WhichQueue));
    }
    if (message->FragsToAck.size() != 0L) {
        ulm_exit((-1, "sharedmemPath::ReturnDesc: message %p "
                  "FragsToAck.size() %ld numfrags %d numsent %d "
                  "numacked %d list %d\n", message, message->FragsToAck.size(),
                  message->numfrags, message->NumSent, message->NumAcked,
                  message->WhichQueue));
    }
#endif                          // _DEBUGQUEUES

    // if this was a bsend (or aborted bsend), then decrement the reference
    // count for the appropriate buffer allocation
    if ( message->sendType == ULM_SEND_BUFFERED && 
        !(message->persistent || message->persistFreeCalled) ) {
        if (message->posted_m.length_m > 0) {
            ulm_bsend_decrement_refcount(
                (ULMRequest_t) message,
                message->bsendOffset);
        }
    }
    // mark descriptor as beeing in the free list
    message->WhichQueue = SENDDESCFREELIST;
    // return descriptor to pool -- always freed by allocating process!
    _ulm_SendDescriptors.returnElement(message, 0);
}

#ifdef ENABLE_RELIABILITY

bool BasePath_t::resend(SendDesc_t *message, int *errorCode)
{
    bool returnValue = false;
    
    // move the timed out frags from FragsToAck back to
    // FragsToSend
    BaseSendFragDesc_t *FragDesc = 0;
    BaseSendFragDesc_t *TmpDesc = 0;
    double              curTime = 0;
    int                 globalDestProc = 0;
    unsigned long long  received_seq_no, delivered_seq_no;
    bool                free_send_resources;
    
    *errorCode = ULM_SUCCESS;
    
    // reset send descriptor earliestTimeToResend
    message->earliestTimeToResend = -1;
    
    for (FragDesc = (BaseSendFragDesc_t *) message->FragsToAck.begin();
         FragDesc != (BaseSendFragDesc_t *) message->FragsToAck.end();
         FragDesc = (BaseSendFragDesc_t *) FragDesc->next)
    {
        
        // reset TmpDesc
        TmpDesc = 0;
        
        // obtain current time
        curTime = dclock();
        
        // retrieve received_largest_inorder_seq
        globalDestProc = FragDesc->globalDestProc();
        received_seq_no = reliabilityInfo->sender_ackinfo[getMemPoolIndex()].process_array
            [globalDestProc].received_largest_inorder_seq;
        delivered_seq_no = reliabilityInfo->sender_ackinfo[getMemPoolIndex()].process_array
            [globalDestProc].delivered_largest_inorder_seq;
        
        free_send_resources = false;
        
        // move frag if timed out and not sitting at the
        // receiver
        if (delivered_seq_no >= FragDesc->fragSequence()) 
        {
            // an ACK must have been dropped somewhere along the way...or
            // it hasn't been processed yet...
            TmpDesc = (BaseSendFragDesc_t *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
            // free all of the other resources after we unlock the frag
            free_send_resources = true;
        } 
        else
        {
            unsigned long long max_multiple = (FragDesc->numTransmits_m < MAXRETRANS_POWEROFTWO_MULTIPLE) ?
            (1 << FragDesc->numTransmits_m) : (1 << MAXRETRANS_POWEROFTWO_MULTIPLE);
            if ((curTime - FragDesc->timeSent_m) >= (RETRANS_TIME * max_multiple)) {
                // resend this frag...
                returnValue = true;
                FragDesc->WhichQueue = fragSendQueue();
                TmpDesc = (BaseSendFragDesc_t *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
                message->FragsToSend.AppendNoLock(FragDesc);
                FragDesc->setSendDidComplete(false);
                (message->NumSent)--;                    
                FragDesc = TmpDesc;
                continue;
            }
            else
            {
                double timeToResend = FragDesc->timeSent_m + (RETRANS_TIME * max_multiple);
                if (message->earliestTimeToResend == -1) {
                    message->earliestTimeToResend = timeToResend;
                } else if (timeToResend < message->earliestTimeToResend) {
                    message->earliestTimeToResend = timeToResend;
                } 
            }
        }
        
        if ( free_send_resources )
        {
            freeResources(message, FragDesc);
        }
        
        // reset FragDesc to previous value, if appropriate, to iterate over list correctly...
        if (TmpDesc) {
            FragDesc = TmpDesc;
        }
    } // end FragsToAck frag descriptor loop
    
    return returnValue;
}

#endif  /* ENABLE_RELIABILITY */

