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

#ifndef _UTSENDDESC
#define _UTSENDDESC

#include "client/ULMClientTypes.h"
#include "util/DblLinkList.h"
#include "internal/constants.h"
#include "path/common/BaseDesc.h"
#include "ulm/ulm.h"

#ifdef RELIABILITY_ON
#include "util/dclock.h"
#endif

//! Base class for managing broadcasts to a process group
class UtsendDesc_t : public Links_t
{
public:
    // State
    //! Queue holding incomplete BaseSendDesc_t send descriptors
    DoubleLinkList incompletePt2PtMessages;

    //! Queue holding unacknowledged but sent BaseSendDesc_t send descriptors
    DoubleLinkList unackedPt2PtMessages;

    //! Pointer to request object associated with this message
    RequestDesc_t *request;

    /*! Array of collective information for broadcast, contains hosts
     *  reference counts, and the comm_root (smallest ProcID on host for
     *  process group) */
    ULMMcastInfo_t *mcastInfo;

    //! size of mcastInfo array
    int mcastInfoLen;

    //! index into mcastInfo array indicating the next send desc. to allocate
    int nextToAllocate;

    //! number of BaseSendDesc_t messages that have been sent
    int messageDoneCount;

    //! number of BaseSendDesc_t message descriptors to allocate
    int numDescsToAllocate;

    //! boolean indicating whether request->messageDone has been set
    bool sendDone;

    //! lock for this message descriptor
    Locks Lock;

#ifdef RELIABILITY_ON
    //! Keeps track of the earliest send time of the frags for retransmits
    double earliestTimeToResend;
#endif

    //! local ProcID of sending process
    int localSourceRank;

    // Pointer to the ULMType_t datatype descriptor associated
    // with this message... NULL if the user is sending raw
    // bytes
    ULMType_t *datatype;

    // address where data for message begins
    void* AppAddr;

    // communicator ID
    int contextID;

    //*************** Methods *****************************************

    //! Constructor
    UtsendDesc_t();

    UtsendDesc_t(int);

    /*! Initialization function - there are a finite number of these
     *  descriptors available, and we are trying to promote reuse */
    int Init(RequestDesc_t *req, ULMMcastInfo_t *ci, int ciLen);

    //! allocate per-host send descriptors
    int allocateSendDescriptors();

    //! General send method used to make progress
    int send(bool lockIncompleteList);

    /*! Method frees any remaining resources and returns
     *  descriptor to free list */
    int freeDescriptor();

#ifdef RELIABILITY_ON
    /*! Method to determine whether we need to retransmit or not */
    bool retransmitP() {
        if (RETRANS_TIME == -1 || earliestTimeToResend == -1 || unackedPt2PtMessages.size() == 0) {
            return false;
        }
        else if (dclock() >= earliestTimeToResend) {
            return true;
        }
        else {
            return false;
        }
    }

    /*! Method to arrange for the appropriate host messages to be retransmitted */
    int reSend();
#endif
};

#endif
