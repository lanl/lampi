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



#ifndef _RELIABILITYINFO
#define _RELIABILITYINFO

#define ULM_64BIT_INT long long

#include "util/Lock.h"
#include "SeqTrackingList.h"
#include "sender_ackinfo.h"

class ReliabilityInfo {

public:

    /* Point-to-Point traffic information */

    //! next pt-2-pt frag sequence number generated on the send side
    //! monotonically increasing from (1 .. (2^64 - 1))
    //  process private memory
    ULM_64BIT_INT *next_frag_seqs;
    Locks *next_frag_seqsLock;

    /*! ACK received and delivered sequence tracking lists for received
     * data messages, and one array of locks for both lists
     *  process private memory
     */
    SeqTrackingList *receivedDataSeqs;
    SeqTrackingList *deliveredDataSeqs;
    Locks *dataSeqsLock;

    //! shared memory arrays to track per-process peer ACK status
    sender_ackinfo_control_t *sender_ackinfo;

    /* Collective traffic information */

    double lastCheckForCollRetransmits;

    //! shared memory host-to-hosts sequencing
    ULM_64BIT_INT *coll_next_frag_seqs;
    Locks *coll_next_frag_seqsLock;

    //! these are fixed sized shared memory SeqTrackingLists
    SeqTrackingList *coll_receivedDataSeqs;
    SeqTrackingList *coll_deliveredDataSeqs;
    Locks *coll_dataSeqsLock;

    //! shared memory arrays to track collective host ACK status
    sender_ackinfo_control_t *coll_sender_ackinfo;

    //! constructor
    ReliabilityInfo();

    // d'tor
    ~ReliabilityInfo();
};

#endif				/* !_RELIABILITYINFO */
