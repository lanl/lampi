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

#include <new>
#include <strings.h>

#include "queue/ReliabilityInfo.h"
#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/log.h"
#include "internal/system.h"


ReliabilityInfo::ReliabilityInfo()
{
#ifdef ENABLE_RELIABILITY

    /* Point-to-Point information initialization */

    // Initialize sender_ackinfo arrays and locks...
    ssize_t LenToAllocate =
        sizeof(sender_ackinfo_control_t) * local_nprocs();
    sender_ackinfo = (sender_ackinfo_control_t *)
        SharedMemoryPools.getMemorySegment(LenToAllocate,
                                           CACHE_ALIGNMENT);
    if (!sender_ackinfo) {
        ulm_exit((-1,
                  "Error: Unable to allocate sender_ackinfo_control_t\n"));
    }
    // initialize array by calling constructors, initializing the locks,
    // and getting a shared memory region for process_array
    LenToAllocate = sizeof(sender_ackinfo_t) * nprocs();
    for (int i = 0; i < local_nprocs(); i++) {
        new(&(sender_ackinfo[i])) sender_ackinfo_control_t;
        sender_ackinfo[i].Lock.init();
        sender_ackinfo[i].process_array = (sender_ackinfo_t *)
            SharedMemoryPools.getMemorySegment(LenToAllocate,
                                               CACHE_ALIGNMENT);
        if (!sender_ackinfo[i].process_array) {
            ulm_exit((-1,
                      "Error: Unable to allocate sender_ackinfo_t array\n"));
        }
        // initialize sequence values to 0 (null/invalid)
        bzero((void *) sender_ackinfo[i].process_array, LenToAllocate);
    }

    //! next frag sequence number to be assigned for a send
    next_frag_seqs = ulm_new(ULM_64BIT_INT, nprocs());
    if (!next_frag_seqs) {
        ulm_exit((-1,
                  "Error: Unable to allocate space for next_frag_seqs\n"));
    }

    for (int i = 0; i < nprocs(); ++i) {
        next_frag_seqs[i] = 1;        // 0 is an null/invalid value
    }

    next_frag_seqsLock = ulm_new(Locks, nprocs());
    if (!next_frag_seqsLock) {
        ulm_exit((-1, "Error: Unable to allocate space for "
                  "next_frag_seqsLock\n"));
    }
    /* !!!!! threaded-lock */
    for (int ele = 0; ele < nprocs(); ele++) {
        next_frag_seqsLock[ele].init();
    }

    // initialize sequence tracking list array for all received message frags
    receivedDataSeqs = ulm_new(SeqTrackingList, nprocs());
    if (!receivedDataSeqs) {
        ulm_exit((-1,
                  "Error: Unable to allocate space for receivedDataSeqs\n"));
    }
    // initialize sequence tracking list array for all delivered message frags
    deliveredDataSeqs = ulm_new(SeqTrackingList, nprocs());
    if (!deliveredDataSeqs) {
        ulm_exit((-1,
                  "Error: Unable to allocate space for "
                 "deliveredDataSeqs\n"));
    }
    // initialize sequence tracking list lock array for both rec'vd+delivered DataSeqs
    dataSeqsLock = ulm_new(Locks, nprocs());
    if (!dataSeqsLock) {
        ulm_exit((-1, "Error: Unable to allocate space for dataSeqsLock\n"));
    }
    /* !!!!! threaded-lock */
    for (int ele = 0; ele < nprocs(); ele++) {
        dataSeqsLock[ele].init();
    }

    /* Collective Information initialization */

    lastCheckForCollRetransmits = -1.0;

    // set up shared memory frag_seqs for flow between hosts/boxes

    coll_next_frag_seqs =
        (ULM_64BIT_INT *) SharedMemoryPools.
        getMemorySegment(sizeof(ULM_64BIT_INT) * lampiState.nhosts,
                         CACHE_ALIGNMENT);
    coll_next_frag_seqsLock =
        (Locks *) SharedMemoryPools.getMemorySegment(sizeof(Locks) *
                                                     lampiState.nhosts,
                                                     CACHE_ALIGNMENT);

    if (!coll_next_frag_seqs || !coll_next_frag_seqsLock) {
        ulm_exit((-1,
                  "Unable to allocate collective next frag_seqs "
                  "structures\n"));
    }

    for (int i = 0; i < lampiState.nhosts; i++) {
        coll_next_frag_seqs[i] = 1;

        // init lock...
        new(&(coll_next_frag_seqsLock[i])) Locks;
        coll_next_frag_seqsLock[i].init();
    }

    // set up shared memory sequence tracking lists for frag_seqs for these flows

    coll_receivedDataSeqs =
        (SeqTrackingList *) SharedMemoryPools.
        getMemorySegment(sizeof(SeqTrackingList) * lampiState.nhosts,
                         CACHE_ALIGNMENT);
    coll_deliveredDataSeqs =
        (SeqTrackingList *) SharedMemoryPools.
        getMemorySegment(sizeof(SeqTrackingList) * lampiState.nhosts,
                         CACHE_ALIGNMENT);
    coll_dataSeqsLock =
        (Locks *) SharedMemoryPools.getMemorySegment(sizeof(Locks) *
                                                     lampiState.nhosts,
                                                     CACHE_ALIGNMENT);

    if (!coll_receivedDataSeqs || !coll_deliveredDataSeqs
        || !coll_dataSeqsLock) {
        ulm_exit((-1,
                  "Unable to allocated collective sequence tracking "
                  "lists structures\n"));
    }

    for (int i = 0; i < lampiState.nhosts; i++) {
        new(&(coll_receivedDataSeqs[i])) SeqTrackingList(50, 0, 0, true);
        if (!coll_receivedDataSeqs[i].getArraySize()) {
            ulm_exit((-1,
                      "Unable to construct coll_receivedDataSeqs[%d]\n",
                      i));
        }

        new(&(coll_deliveredDataSeqs[i])) SeqTrackingList(50, 0, 0, true);
        if (!coll_deliveredDataSeqs[i].getArraySize()) {
            ulm_exit((-1,
                      "Unable to construct "
                     "coll_deliveredDataSeqs[%d]\n", i));
        }

        new(&(coll_dataSeqsLock[i])) Locks;
        coll_dataSeqsLock[i].init();
    }

    // set up sender ackinfo structures to hold collective peer frag_seq received/delivered info

    coll_sender_ackinfo =
        (sender_ackinfo_control_t *) SharedMemoryPools.
        getMemorySegment(sizeof(sender_ackinfo_control_t),
                         CACHE_ALIGNMENT);
    if (!coll_sender_ackinfo) {
        ulm_exit((-1, "Unable to allocate coll_sender_ackinfo\n"));
    }
    new(coll_sender_ackinfo) sender_ackinfo_control_t;
    coll_sender_ackinfo->Lock.init();
    coll_sender_ackinfo->process_array =
        (sender_ackinfo_t *) SharedMemoryPools.
        getMemorySegment(sizeof(sender_ackinfo_t) * lampiState.nhosts,
                         CACHE_ALIGNMENT);
    if (!coll_sender_ackinfo->process_array) {
        ulm_exit((-1,
                  "Unable to allocate process_array of "
                  "coll_sender_ackinfo\n"));
    }
    bzero((void *) coll_sender_ackinfo->process_array,
          sizeof(sender_ackinfo_t) * lampiState.nhosts);

#endif
}


ReliabilityInfo::~ReliabilityInfo()
{
#ifdef ENABLE_RELIABILITY
    if (next_frag_seqs) {
        ulm_delete(next_frag_seqs);
        next_frag_seqs = 0;
    }
    if (next_frag_seqsLock) {
        ulm_delete(next_frag_seqsLock);
        next_frag_seqsLock = 0;
    }
    if (receivedDataSeqs) {
        ulm_delete(receivedDataSeqs);
        receivedDataSeqs = 0;
    }
    if (deliveredDataSeqs) {
        ulm_delete(deliveredDataSeqs);
        deliveredDataSeqs = 0;
    }
    if (dataSeqsLock) {
        ulm_delete(dataSeqsLock);
        dataSeqsLock = 0;
    }
#endif
}
