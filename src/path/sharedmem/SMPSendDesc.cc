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

#include <strings.h>

#include "queue/globals.h"
#include "util/Utility.h"
#include "path/sharedmem/SMPSendDesc.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "internal/buffer.h"
#include "internal/log.h"
#include "internal/system.h"
#include "os/numa.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

// maximum number of outstanding framgments per message
static int maxOutstandingSMPFrags = 30;

// ctor
SMPSendDesc_t::SMPSendDesc_t(int plIndex)
{
    //
    // initialize locks
    //
    // SMPSendDesc_t lock
    SenderLock.init();

    // list locks  (these are all accessed while the SMPSendDesc_t is locked
    //   therefore these are no-op locks

    freeFrags.Lock.init();
    fragsReadyToSend.Lock.init();

    //
    // set pointer of first frag
    //
    // compute address of first frag
    size_t offsetToFirstFrag = sizeof(SMPSendDesc_t);
    firstFrag = (SMPFragDesc_t *) ((char *) this + offsetToFirstFrag);

    // placement new
    new(firstFrag) SMPFragDesc_t (plIndex);

    // compute address of first frag payload - will never change
    size_t offsetToPayload =
        (((sizeof(SMPSendDesc_t) + sizeof(SMPFragDesc_t) - 1) /
          CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
    void *firstFragPayloadAddr =
        (void *) ((char *) this + offsetToPayload);

    // set the payload address in the first frag
    firstFrag->addr_m = firstFragPayloadAddr;

    // fill in pointer to send descriptor
    firstFrag->SendingHeader_m.SMP = (SMPSendDesc_t *) this;
}


// initialization function - first frag is posted on the receive side
int SMPSendDesc_t::Init(int DestProc, size_t len,
                        int usr_tag, int comm, ULMType_t *dtype)
{
    // For all the pages in the send request  - need a minimum of 1 page
    unsigned int FragCount;
    if (len <= SMPFirstFragPayload) {
        FragCount = 1;
    } else {
        FragCount =
            ((len - SMPFirstFragPayload) + SMPSecondFragPayload - 1) /
            SMPSecondFragPayload + 1;
    }

    // Allocate irecv descriptors - these will be put directly into the
    //   queues, for processing by the receive side.
    //   we assume that the recyled descriptors have an empty queue.

    // initialize descriptor
    PostedLength = len;
    dstProcID_m = DestProc;
    tag_m = usr_tag;
    communicator = comm;
    numfrags = FragCount;
    NumAcked = 0;
    NumSent = 0;
    matchedRecv = 0;
    sendDone = (sendType == ULM_SEND_BUFFERED) ? 1 : 0;

    // set datatype, typemap index
    datatype = dtype;

    // get communicator pointer
    Communicator *commPtr = (Communicator *) communicators[communicator];

    // recv process's queue
    int SortedRecvFragsIndex =
        commPtr->remoteGroup->mapGroupProcIDToGlobalProcID[dstProcID_m];
    SortedRecvFragsIndex = global_to_local_proc(SortedRecvFragsIndex);

    // we're sending to ourselves and we're the only shared memory process so we need
    // to set the boolean latch to true so that we can pick up the message
    // in ulm_make_progress
    extern bool _ulm_checkSMPInMakeProgress;
    if (!_ulm_checkSMPInMakeProgress
        && (SortedRecvFragsIndex == lampiState.local_rank)) {
        _ulm_checkSMPInMakeProgress = true;
    }
    // process as much as possible of the first frag

    // get first fragement descriptor - when a send is initialized,
    //   freeFrags is guaranteed to be non-empty

    // set index
    firstFrag->fragIndex_m = 0;

    // set tmap index
    firstFrag->tmapIndex_m = 0;

    // set source process
    int myRank = commPtr->localGroup->ProcID;
    firstFrag->srcProcID_m = myRank;

    // set destination process
    firstFrag->dstProcID_m = dstProcID_m;

    // set user tag and communicator and related fields
    firstFrag->tag_m = tag_m;
    firstFrag->ctx_m = communicator;

#ifdef _DEBUGQUEUES
    // set sequence number
    firstFrag->isendSeq_m = isendSeq_m;
#endif                          // _DEBUGQUEUES

    // set message length
    firstFrag->msgLength_m = PostedLength;

    //
    // special case - 0 byte message
    //
    if (PostedLength == 0) {
        // set frag length
        firstFrag->length_m = 0;
        NumSent = 1;
        // don't allow frag payload to be read yet, no buffer allocated
        //   and data not copied in
        firstFrag->okToReadPayload_m = false;
        // memory barrier before posting...
        mb();

        // update number of descriptors allocated
        NumFragDescAllocated = 1;

        // ZERO LENGTH
        // append to destination list

        int sizeOfSendsToPost = SMPSendsToPost[(local_myproc())]->size();
        int senderID = local_myproc();
        int receiverID = SortedRecvFragsIndex;
        if (sizeOfSendsToPost == 0) {
            int slot;
            if (usethreads()) {
                slot =
                    SharedMemIncomingFrags[(senderID)][(receiverID)]->
                    writeToHead(&firstFrag);
                if (slot == CB_ERROR) {
                    SMPSendsToPost[(senderID)]->Append((firstFrag));
                }
            } else {
                mb();
                slot =
                    SharedMemIncomingFrags[(senderID)][(receiverID)]->
                    writeToHeadNoLock(&firstFrag);
                if (slot == CB_ERROR) {
                    SMPSendsToPost[(senderID)]->AppendNoLock((firstFrag));
                }
                mb();
            }
        } else {
            if (usethreads())
                SMPSendsToPost[(senderID)]->Append((firstFrag));
            else {
                mb();
                SMPSendsToPost[(senderID)]->AppendNoLock((firstFrag));
                mb();
            }
        }


        // check to see if send is done (buffered already "done";
        // synchronous sends are not done until the first frag is acked)
        if (!sendDone && sendType != ULM_SEND_SYNCHRONOUS) {
            requestDesc->messageDone = true;
            sendDone = 1;
        }
        // return
        return ULM_SUCCESS;

    }
    // fill in frag size
    size_t LeftToSend = PostedLength;
    if (LeftToSend > SMPFirstFragPayload) {
        firstFrag->length_m = SMPFirstFragPayload;
    } else {
        firstFrag->length_m = LeftToSend;
    }

    // set sequential offset
    firstFrag->seqOffset_m = 0;


#ifdef _DEBUGQUEUES
    // set isend sequence number
    firstFrag->isendSeq_m = isendSeq_m;
#endif                          // _DEBUGQUEUES

#ifdef _DEBUGQUEUES
    // set flag indicating which list frag is in
    firstFrag->WhichQueue = SMPFRAGSTOSEND;
#endif                          // _DEBUGQUEUES

    // don't allow frag payload to be read yet, no buffer allocated
    //   and data not copied in
    firstFrag->okToReadPayload_m = false;

    // initialize descriptor flag
    firstFrag->flags_m = 0;
    // memory barrier before posting...
    mb();

    // append to destination list
    int sizeOfSendsToPost = SMPSendsToPost[(local_myproc())]->size();
    int senderID = local_myproc();
    int receiverID = SortedRecvFragsIndex;
    if (sizeOfSendsToPost == 0) {
        int slot;
        if (usethreads()) {
            slot = SharedMemIncomingFrags[(senderID)][(receiverID)]->
                writeToHead(&firstFrag);
            if (slot == CB_ERROR) {
                SMPSendsToPost[(senderID)]->Append((firstFrag));
            }
        } else {
            mb();
            slot = SharedMemIncomingFrags[(senderID)][(receiverID)]->
                writeToHeadNoLock(&firstFrag);
            if (slot == CB_ERROR) {
                SMPSendsToPost[(senderID)]->AppendNoLock((firstFrag));
            }
            mb();
        }

    } else {
        if (usethreads())
            SMPSendsToPost[(senderID)]->Append((firstFrag));
        else {
            mb();
            SMPSendsToPost[(senderID)]->AppendNoLock((firstFrag));
            mb();
        }
    }

    // update number of descriptors allocated
    NumFragDescAllocated = 1;


    // set flag indicating memory is allocated
    firstFrag->flags_m |= IO_SOURCEALLOCATED;
    firstFrag->SendingHeader_m.SMP->CopyToULMBuffers(firstFrag);

    // lock the sender to make sure that NumSent does not change until
    //   we have set matchedRecv on the receive side
    wmb();
    firstFrag->okToReadPayload_m = true;
    /* memory barrier - need to make sure that okToReadPayload is set
     * before NumSent
     */
    wmb();
    NumSent = 1;

    // check to see if send is done (buffered sends are already
    // "done, and synchronous send are done when the first frag
    // is acknowledged)
    if (!sendDone &&
        (NumSent == numfrags) && (sendType != ULM_SEND_SYNCHRONOUS)) {
        requestDesc->messageDone = true;
        sendDone = 1;
    }

    return ULM_SUCCESS;
}


// copy data into library buffers
void SMPSendDesc_t::CopyToULMBuffers(SMPFragDesc_t *descriptor)
{
    SMPFragDesc_t *recvFrag = (SMPFragDesc_t *) descriptor;
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (datatype == NULL || datatype->layout == CONTIGUOUS) {
        src_addr = ((unsigned char *) AppAddr) + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = recvFrag->length_m;
        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
    } else {                    // data is non-contiguous
        ULMType_t *dtype = datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int tm_init = recvFrag->tmapIndex_m;
        int init_cnt = recvFrag->seqOffset_m / dtype->packed_size;
        int tot_cnt = recvFrag->msgLength_m / dtype->packed_size;
        unsigned char *start_addr = ((unsigned char *) AppAddr)
            + init_cnt * dtype->extent;
        int dtype_cnt, ti;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size
            - tmap[tm_init].seq_offset + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size
            + tmap[tm_init].seq_offset - recvFrag->seqOffset_m;
        len_to_copy =
            (len_to_copy >
             recvFrag->length_m) ? recvFrag->length_m : len_to_copy;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = ((unsigned char *) recvFrag->addr_m)
                    + len_copied;

                len_to_copy =
                    (recvFrag->length_m - len_copied >=
                     tmap[ti].size) ? tmap[ti].size : recvFrag->length_m -
                    len_copied;
                if (len_to_copy == 0)
                    return;

                bcopy(src_addr, dest_addr, len_to_copy);
                len_copied += len_to_copy;
            }

            tm_init = 0;
            start_addr += dtype->extent;
        }
    }
}


// copy data into library buffers
void SMPSendDesc_t::CopyToULMBuffers(SMPSecondFragDesc_t * descriptor)
{
    SMPSecondFragDesc_t *recvFrag = (SMPSecondFragDesc_t *) descriptor;
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (datatype == NULL || datatype->layout == CONTIGUOUS) {
        src_addr = ((unsigned char *) AppAddr) + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = recvFrag->length_m;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);

    } else {                    // data is non-contiguous
        ULMType_t *dtype = datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int tm_init = recvFrag->tmapIndex_m;
        int init_cnt = recvFrag->seqOffset_m / dtype->packed_size;
        int tot_cnt = recvFrag->msgLength_m / dtype->packed_size;
        unsigned char *start_addr = ((unsigned char *) AppAddr)
            + init_cnt * dtype->extent;
        int dtype_cnt, ti;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size
            - tmap[tm_init].seq_offset + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr_m;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size
            + tmap[tm_init].seq_offset - recvFrag->seqOffset_m;
        len_to_copy =
            (len_to_copy >
             recvFrag->length_m) ? recvFrag->length_m : len_to_copy;

        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = ((unsigned char *) recvFrag->addr_m)
                    + len_copied;

                len_to_copy =
                    (recvFrag->length_m - len_copied >=
                     tmap[ti].size) ? tmap[ti].size : recvFrag->length_m -
                    len_copied;
                if (len_to_copy == 0)
                    return;

                MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
                len_copied += len_to_copy;
            }

            tm_init = 0;
            start_addr += dtype->extent;
        }
    }
}


// continue sending, may be called multiple times per send descriptor
int SMPSendDesc_t::Send()
{
    //
    // determine which pool to use
    //
    // group pointer
    Communicator *commPtr = (Communicator *) communicators[communicator];

    // recv process's queue
    int SortedRecvFragsIndex =
        commPtr->remoteGroup->mapGroupProcIDToGlobalProcID[dstProcID_m];
    SortedRecvFragsIndex = global_to_local_proc(SortedRecvFragsIndex);

#ifdef USE_DEST_MEM
    // locality index
    int memPoolIndex = global_to_local_proc(memPoolIndex);
#else
    // locality index
    int memPoolIndex = getMemPoolIndex();
#endif

    //
    // process the rest of the frags
    //

    //
    // allocate frag descriptors, if need be - the assumption is that
    //   these descriptors will be allocated sequentially, and therefore
    //   we can use the order to get offsets with in the message.
    //
    if (NumFragDescAllocated < numfrags) {
        int NumDescToAllocate = numfrags - NumFragDescAllocated;
        for (int ndesc = 0; ndesc < NumDescToAllocate; ndesc++) {

            int waitOnAck =
                (requestDesc->sendType == ULM_SEND_SYNCHRONOUS);

            // slow down the send
            if ((maxOutstandingSMPFrags != -1) &&
                (NumFragDescAllocated - NumAcked >=
                 (unsigned) maxOutstandingSMPFrags)) {
                clearToSend_m = false;
            } else if (!(waitOnAck && NumAcked == 0)) {
                clearToSend_m = true;
            }
            // Request To Send/Clear To Send
            if (!clearToSend_m) {
                break;
            }
            // request desriptor
            SMPSecondFragDesc_t *FragDesc;

            // first try and get "cached" descriptor
            FragDesc = (SMPSecondFragDesc_t *) freeFrags.GetLastElement();
            if (!FragDesc) {

                // get one from the free pool

                int errorCode;
                FragDesc = (SMPSecondFragDesc_t *)
                    (SMPSecondFragDesc_t *) SMPFragPool.
                    getElement(memPoolIndex, errorCode);
                if (errorCode == ULM_ERR_OUT_OF_RESOURCE)
                    return errorCode;
                if (FragDesc == (SMPSecondFragDesc_t *) 0) {
                    break;
                }

            }
            // sanity check
#ifdef _DEBUGQUEUES
            if (FragDesc->WhichQueue != SMPFREELIST) {
                ulm_exit((-1, " FragDesc->WhichQueue != SMPFREELIST "
                          " :: FragDesc->WhichQueue %d\n",
                          FragDesc->WhichQueue));
            }
#endif                          /* _DEBUGQUEUES */
            assert(FragDesc->WhichQueue == SMPFREELIST);

            // fill in pointer to send descriptor
            FragDesc->SendingHeader_m.SMP = (SMPSendDesc_t *) this;

            // set message length
            FragDesc->msgLength_m = PostedLength;

            // fill in frag size
            size_t LeftToSend = (PostedLength - SMPFirstFragPayload) -
                SMPSecondFragPayload * (NumFragDescAllocated - 1);
            if (LeftToSend > (size_t) SMPSecondFragPayload) {
                FragDesc->length_m = SMPSecondFragPayload;
            } else {
                FragDesc->length_m = LeftToSend;
            }

            // set sequential offset - NumFragDescAllocated has not yet been
            //  incremented
            FragDesc->seqOffset_m = SMPFirstFragPayload +
                (NumFragDescAllocated - 1) * SMPSecondFragPayload;

            // set typemap index if data is non-contiguous
            if (datatype != NULL && datatype->layout == NON_CONTIGUOUS) {
                int dtype_cnt = FragDesc->seqOffset_m
                    / datatype->packed_size;
                size_t data_copied = dtype_cnt * datatype->packed_size;
                ssize_t data_remaining =
                    (ssize_t) (FragDesc->seqOffset_m - data_copied);
                int ti;

                /* check validity for the following loop */
                assert((dtype_cnt + 1) * datatype->packed_size >
                       FragDesc->seqOffset_m);

                FragDesc->tmapIndex_m = datatype->num_pairs - 1;
                for (ti = 0; ti < datatype->num_pairs; ti++) {
                    if (datatype->type_map[ti].seq_offset ==
                        data_remaining) {
                        FragDesc->tmapIndex_m = ti;
                        break;
                    } else if (datatype->type_map[ti].seq_offset >
                               data_remaining) {
                        FragDesc->tmapIndex_m = ti - 1;
                        break;
                    }
                }
            } else {
                FragDesc->tmapIndex_m = 0;
            }

#ifdef _DEBUGQUEUES
            // set flag indicating which list frag is in
            FragDesc->WhichQueue = SMPFRAGSTOSEND;
#endif                          // _DEBUGQUEUES

            // update number of descriptors allocated
            NumFragDescAllocated++;

            // set flag indicating memory is allocated
            FragDesc->SendingHeader_m.SMP->CopyToULMBuffers(FragDesc);
            // lock fragsReadyToSend
            fragsReadyToSend.Lock.lock();

            // queue frag
            if (matchedRecv) {
                // match already made
                FragDesc->matchedRecv_m = matchedRecv;
                wmb();
                SMPMatchedFrags[SortedRecvFragsIndex]->Append(FragDesc);
            } else {
                wmb();
                fragsReadyToSend.AppendNoLock(FragDesc);
            }

            NumSent++;
            // unlock
            fragsReadyToSend.Lock.unlock();

        }                       // end ndesc loop
    }                           // end NumFragDescAllocated < numfrags


    // check to see if send is done (buffered sends are already "done";
    // and synchronous sends are done when the first frag is
    // acked (and everything else has been sent...)
    if (!sendDone &&
        (NumSent == numfrags) &&
        (requestDesc->sendType != ULM_SEND_SYNCHRONOUS)) {
        requestDesc->messageDone = true;
        sendDone = 1;
    }
    // return
    return ULM_SUCCESS;
}


// return descriptor to the free pool
void SMPSendDesc_t::ReturnDesc()
{
    // if this was a bsend (or aborted bsend), then decrement the reference
    // count for the appropriate buffer allocation
    if (sendType == ULM_SEND_BUFFERED) {
        if (PostedLength > 0) {
            ulm_bsend_decrement_refcount((ULMRequestHandle_t) requestDesc,
                                         bsendOffset);
        }
    }
    // mark descriptor as in the free list
    WhichQueue = SENDDESCFREELIST;

    // return descriptor to the pool
    if (usethreads()) {
        SMPSendDescs.returnElement(this, getMemPoolIndex());
    } else {
        SMPSendDescs.returnElementNoLock(this, getMemPoolIndex());
    }
}
