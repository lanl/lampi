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

#include "queue/globals.h"
#include "path/mcast/CollSharedMemGlobals.h"
#include "path/mcast/LocalCollFragDesc.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/mcast/utsendDesc.h"
#include "internal/constants.h"
#include "path/common/BaseDesc.h"
#include "ulm/ulm.h"

Links_t *UtsendDesc_t::allocLocalFrag(ULMMcastInfo_t & co)
{
    int nElementsAddedAtOnce = 0;
    int memPoolIndex = 0;
    int errorCode;
    size_t lenAdded = 0;
    char *CurrentLoc;
    ssize_t eleSize = 0;

    memPoolIndex = getMemPoolIndex();

    LocalCollFragDesc *FragDesc =
        (LocalCollFragDesc *) LocalCollFragDescriptors.
        RequestElement(memPoolIndex);
    if (!FragDesc) {
        if (LocalCollFragDescriptors.freeLists_m[memPoolIndex]->lock_m.
            trylock() == 1) {
            void *basePtr =
                LocalCollFragDescriptors.GetChunkOfMemory(memPoolIndex,
                                                          &lenAdded,
                                                          &errorCode);
            if (basePtr != (void *) (-1L)) {
                CurrentLoc = (char *) basePtr;
                nElementsAddedAtOnce =
                    LocalCollFragDescriptors.nElementsPerChunk_m;
                eleSize = sizeof(LocalCollFragDesc);
                for (int desc = 0; desc < nElementsAddedAtOnce; desc++) {
                    new(CurrentLoc) LocalCollFragDesc;
                    // initialize locks
                    LocalCollFragDesc *curlock =
                        (LocalCollFragDesc *) CurrentLoc;
                    curlock->Lock.init();
                    CurrentLoc += eleSize;
                }               //  for (int desc=0 ; desc < ....
                LocalCollFragDescriptors.AppendToFreeList(memPoolIndex,
                                                          (char *)
                                                          basePtr);
            }
            LocalCollFragDescriptors.freeLists_m[memPoolIndex]->lock_m.
                unlock();
            return 0;
        }                       //if (LocalCollFragDescriptors.freeLists_m[Loca

    }                           // if (FragDesc != NULL)

#ifdef _DEBUGQUEUES
    if (FragDesc->WhichQueue != SMPFREELIST) {
        ulm_exit((-1, "Error:  FragDesc->WhichQueue (%d) != SMPFREELIST\n",
                  FragDesc->WhichQueue));
    }
#endif
    assert(FragDesc->WhichQueue == SMPFREELIST);

    // fill in pointer to send descriptor
    //FragDesc->SendingHeader.Coll = this;
    FragDesc->SendingHeader = this;

    // set message length
    FragDesc->msgLength_m = request->posted_m.length_m;

    // fill in frag size
    size_t LeftToSend =
        request->posted_m.length_m -
        SizeLargestSMPSharedMemBucket * fragIndex;
    if (LeftToSend > SizeLargestSMPSharedMemBucket) {
        FragDesc->length_m = SizeLargestSMPSharedMemBucket;
    } else {
        FragDesc->length_m = LeftToSend;
    }

    // set index
    FragDesc->fragIndex_m = fragIndex;

    // set maxSegSize_m
    FragDesc->maxSegSize_m = SizeLargestSMPSharedMemBucket;

    // set sequential offset
    FragDesc->seqOffset_m = FragDesc->fragIndex_m *
        SizeLargestSMPSharedMemBucket;

    // calculate the typemap index, if the data is non-contiguous
    if (datatype != NULL && datatype->layout == NON_CONTIGUOUS) {
        int dtype_cnt = FragDesc->seqOffset_m / datatype->packed_size;
        size_t data_copied = dtype_cnt * datatype->packed_size;
        size_t data_remaining = FragDesc->seqOffset_m - data_copied;
        int ti;

        // check validity for the following loop
        assert((dtype_cnt + 1) * datatype->packed_size >
               FragDesc->seqOffset_m);

        FragDesc->tmap_index = datatype->num_pairs - 1;
        for (ti = 0; ti < datatype->num_pairs; ti++) {
            if (datatype->type_map[ti].seq_offset == data_remaining) {
                FragDesc->tmap_index = ti;
                break;
            } else if (datatype->type_map[ti].seq_offset > data_remaining) {
                FragDesc->tmap_index = ti - 1;
                break;
            }
        }
    } else {
        FragDesc->tmap_index = 0;
    }

    // set source process
    FragDesc->srcProcID_m = myproc();

    // set destination process
    FragDesc->dstProcID_m =
     indent: Standard input:143: Warning:old style assignment ambiguity in "=&".  Assuming "= &"

   communicators[request->contextID]->localGroup->
        groupHostData[communicators[request->contextID]->localGroup->
                      groupHostDataLookup[co.hostID]].destProc;

    // set user tag and communicator
    FragDesc->tag_m = request->posted_m.UserTag_m;
    FragDesc->messageType = request->messageType;
    FragDesc->contextID = request->contextID;

    // set isend sequence number
    FragDesc->isendSeq_m = isendSeq_m;


    // set refCnt
    FragDesc->refCnt = co.nGroupProcIDOnHost;
    // initialize descriptor flag
    FragDesc->flags = 0;
    if (FragDesc->length_m == 0) {
        FragDesc->flags = IO_SOURCEALLOCATED;
        FragDesc->addr = 0;
    }
    // set flag indicating which list frag is in
    FragDesc->WhichQueue = SMPFRAGSTOSEND;

    // increment UtsendDesc_t dataOffset properly
    dataOffset += FragDesc->length_m;

    return FragDesc;
}

int UtsendDesc_t::sendLocalFrags()
{
    Communicator *commPtr = NULL;
    SMPSharedMem_logical_dev_t *dev = &SMPSharedMemDevs[getMemPoolIndex()];
    commPtr = (Communicator *) communicators[request->contextID];
    int destRank =
        global_to_local_proc(commPtr->localGroup->
                             groupHostData[commPtr->localGroup->
                                           hostIndexInGroup].destProc);
    void *v;
    int numsent = 0;

    //   Loop through the frags
    for (LocalCollFragDesc *
         RecvDesc = (LocalCollFragDesc *) LocalFragsToSend.begin();
         RecvDesc != (LocalCollFragDesc *) LocalFragsToSend.end();
         RecvDesc = (LocalCollFragDesc *) RecvDesc->next) {
        // we will succeed on the first try if we get the source shared
        // memory to copy the user data into
        if (RecvDesc->length_m != 0) {
            v = dev->MemoryBuckets.ULMMalloc(RecvDesc->length_m);
            if (v != (void *) -1L) {
                RecvDesc->addr = v;
                RecvDesc->flags |= IO_SOURCEALLOCATED;
                CopyInColl(RecvDesc);

            }
        } else {
            RecvDesc->addr = NULL;
            RecvDesc->flags |= IO_SOURCEALLOCATED;
        }

        if ((RecvDesc->flags & IO_SOURCEALLOCATED) == IO_SOURCEALLOCATED) {
            // set WhichQueue indicator
            RecvDesc->WhichQueue = SORTEDRECVFRAGS;

            // remove descriptor from list of isend recv descriptors allocated with
            //   this message
            LocalCollFragDesc *TmpDesc =
                (LocalCollFragDesc *) LocalFragsToSend.
                RemoveLink(RecvDesc);
            // add descriptor to list of _ulm_SortedPt2PtRecvFrags
            _ulm_CommunicatorRecvFrags[destRank]->AppendAsWriter(RecvDesc);
            RecvDesc = TmpDesc;
            numsent++;

            // increment NumSent - no need to lock, since only sending process
            //   can update numsent :: this is a "reliable network"

        }

    }
    return numsent;
}


void UtsendDesc_t::CopyInColl(void *Descriptor)
{
    LocalCollFragDesc *recvFrag = (LocalCollFragDesc *) Descriptor;
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied;

    // determine if data is contiguous
    if (datatype == NULL || datatype->layout == CONTIGUOUS) {
        src_addr = ((unsigned char *) AppAddr)
            + recvFrag->fragIndex_m * SizeLargestSMPSharedMemBucket;
        dest_addr = (unsigned char *) recvFrag->addr;
        len_to_copy = recvFrag->length_m;

        bcopy(src_addr, dest_addr, len_to_copy);
    } else {                    // data is non-contiguous
        ULMType_t *dtype = datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int tm_init = recvFrag->tmap_index;
        int init_cnt = recvFrag->seqOffset_m / dtype->packed_size;
        int tot_cnt = recvFrag->msgLength_m / dtype->packed_size;
        unsigned char *start_addr = ((unsigned char *) AppAddr)
            + dtype->lower_bound + init_cnt * dtype->extent;
        int dtype_cnt, ti;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size
            - tmap[tm_init].seq_offset + recvFrag->seqOffset_m;
        dest_addr = (unsigned char *) recvFrag->addr;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size
            + tmap[tm_init].seq_offset - recvFrag->seqOffset_m;

        bcopy(src_addr, dest_addr, len_to_copy);
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = ((unsigned char *) recvFrag->addr)
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
