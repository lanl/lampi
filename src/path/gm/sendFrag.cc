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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/MemFunctions.h"
#include "internal/types.h"
#include "path/gm/sendFrag.h"
#include "os/atomic.h"


bool gmSendFragDesc::init(int globalDestProc,
                          int tmapIndex,
                          SendDesc_t *parentSendDesc,
                          size_t seqOffset,
                          size_t length,
                          BasePath_t *failedPath,
                          BaseSendFragDesc_t::packType packed,
                          int numTransmits)
{
    gmFragBuffer *buf = 0;
    gmHeaderData *headerp;
    void *payloadp;
    int rc;

    if (!initialized_m) {

        BaseSendFragDesc_t::init();
        
        globalDestProc_m = globalDestProc;
        tmapIndex_m = tmapIndex;
        parentSendDesc_m = parentSendDesc;
        seqOffset_m = seqOffset;
        length_m = length;
        failedPath_m = failedPath;
        packed_m = packed;
        numTransmits_m = numTransmits;

        timeSent_m = -1;
        initialized_m = true;
        didRecvAck_m = false;
        gmSends_m = 0;
    }

    if (!currentSrcAddr_m) {

        // choose a NIC, allocate a buffer

        for (int i = 1; i <= gmState.nDevsAllocated; i++) {
            dev_m = (gmState.lastDev + i) % gmState.nDevsAllocated;
            if (gmState.localDevList[dev_m].devOK &&
                gmState.localDevList[dev_m].remoteDevList[globalDestProc_m].node_id != (unsigned int) -1) {

                if (usethreads()) {
                    gmState.localDevList[dev_m].Lock.lock();
                }
                buf = gmState.localDevList[dev_m].bufList.getElementNoLock(0, rc);
                if (usethreads()) {
                    gmState.localDevList[dev_m].Lock.unlock();
                }
                if (rc != ULM_SUCCESS) {
                    continue;
                }
                currentSrcAddr_m = (void *) buf;
                gmState.lastDev = dev_m;
                break;
            }
        }
        // check that we got a buffer
        if (!currentSrcAddr_m) {
            return false;
        }

        // build header

        headerp = &(buf->header.data);
        payloadp = (void *) buf->payload;

        if (parentSendDesc_m->sendType == ULM_SEND_SYNCHRONOUS) {
            headerp->ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE
                (parentSendDesc_m->ctx_m,MSGTYPE_PT2PT_SYNC);
        } else {
            headerp->ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE
                (parentSendDesc_m->ctx_m,MSGTYPE_PT2PT);
        }

        headerp->common.type = MESSAGE_DATA;
        headerp->user_tag = parentSendDesc_m->posted_m.tag_m;
        headerp->senderID = myproc();
        headerp->destID = globalDestProc_m;
        headerp->dataLength = length_m;
        headerp->msgLength = parentSendDesc_m->posted_m.length_m;
        fragSeq_m = 0;  // fix later for reliability!!!!
#if ENABLE_RELIABILITY
        parentSendDesc_m->path_m->initFragSeq(this);
#endif
        headerp->frag_seq = fragSeq_m;
        
        headerp->isendSeq_m = parentSendDesc_m->isendSeq_m;
        headerp->sendFragDescPtr.ptr = (void *) this;
        headerp->dataSeqOffset = seqOffset_m;
        headerp->checksum = 0;
    }

    buf = (gmFragBuffer *)currentSrcAddr_m;
    headerp = &(buf->header.data);
    payloadp = (void *) buf->payload;

    // copy and/or pack data

    if (tmapIndex_m < 0) {        // contiguous data

        unsigned char *src = (unsigned char *) parentSendDesc_m->addr_m + seqOffset_m;

        if (gmState.doChecksum) {
            if (usecrc()) {
                headerp->dataChecksum = bcopy_uicrc(src, payloadp, length_m, length_m);
            } else {
                headerp->dataChecksum = bcopy_uicsum(src, payloadp, length_m, length_m);
            }
        } else {
            MEMCOPY_FUNC(src, buf->payload, length_m);
        }

    } else {                     // non-contiguous data

        unsigned char *src_addr;
        unsigned char *dest_addr = (unsigned char *) payloadp;
        size_t len_to_copy, len_copied;
        ULMType_t *dtype = parentSendDesc_m->datatype;
        ULMTypeMapElt_t *tmap = dtype->type_map;
        int dtype_cnt, ti;
        int tm_init = tmapIndex_m;
        int init_cnt = seqOffset_m / dtype->packed_size;
        int tot_cnt = parentSendDesc_m->posted_m.length_m / dtype->packed_size;
        unsigned char *start_addr = (unsigned char *) parentSendDesc_m->addr_m + init_cnt * dtype->extent;
        unsigned int csum = 0, ui1 = 0, ui2 = 0;

        // handle first typemap pair
        src_addr = start_addr
            + tmap[tm_init].offset
            - init_cnt * dtype->packed_size - tmap[tm_init].seq_offset + seqOffset_m;
        len_to_copy = tmap[tm_init].size
            + init_cnt * dtype->packed_size + tmap[tm_init].seq_offset - seqOffset_m;
        len_to_copy = (len_to_copy > length_m) ? length_m : len_to_copy;

        if (gmState.doChecksum) {
            if (usecrc()) {
                csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, CRC_INITIAL_REGISTER);
            } else {
                csum = bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
            }
        } else {
            MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
        }
        len_copied = len_to_copy;

        tm_init++;
        for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
            for (ti = tm_init; ti < dtype->num_pairs; ti++) {
                src_addr = start_addr + tmap[ti].offset;
                dest_addr = (unsigned char *) payloadp + len_copied;
                len_to_copy = (length_m - len_copied >= tmap[ti].size) ?
                    tmap[ti].size : length_m - len_copied;
                if (len_to_copy == 0) {
                    break;
                }
                if (gmState.doChecksum) {
                    if (usecrc()) {
                        csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, csum);
                    } else {
                        csum += bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
                    }
                } else {
                    MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
                }
                len_copied += len_to_copy;
            }

            tm_init = 0;
            start_addr += dtype->extent;
        }

        headerp->dataChecksum = csum;
    }
    
    if (ENABLE_RELIABILITY) {
        if (gmState.doChecksum)  {
            headerp->checksum
                = BasePath_t::headerChecksum(headerp,
                                             sizeof(gmHeader) - sizeof(ulm_uint32_t),
                                             GM_HDR_WORDS);        
        }
    }
    
    return true;
}


void gmSendFragDesc::freeResources(double timeNow, SendDesc_t *bsd)
{

    if ( GMFRAGFREELIST == WhichQueue )
        return;		// already freed
    if ( gmSends_m != 0 )
        return;

    if (NULL != bsd) {
        bsd->clearToSend_m = true;

        // remove frag descriptor from list of frags to be acked
        if (WhichQueue == GMFRAGSTOACK) {
            bsd->FragsToAck.RemoveLinkNoLock((Links_t *) this);
        }
        else if (ENABLE_RELIABILITY && WhichQueue == GMFRAGSTOSEND) {
            bsd->FragsToSend.RemoveLinkNoLock((Links_t *) this);
            // increment NumSent since we were going to send this again...
            (bsd->NumSent)++;
        }
        else {
            ulm_exit(("Error: gmPath::callback: Frag on %d queue\n",
                      WhichQueue));
        }
    }

    if (ENABLE_RELIABILITY) {
        // set frag_seq value to 0/null/invalid to detect duplicate ACKs
        fragSeq_m = 0;
    }

    // return all sfd send resources
    WhichQueue = GMFRAGFREELIST;

    // return fragment buffer to free list
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.lock();
        gmState.localDevList[dev_m].bufList.returnElementNoLock((Links_t *) currentSrcAddr_m, 0);
        gmState.localDevList[dev_m].Lock.unlock();
    } else {
        gmState.localDevList[dev_m].bufList.returnElementNoLock((Links_t *) currentSrcAddr_m, 0);
    }

    // clear fields that will be initialized in init()
    currentSrcAddr_m = NULL;
    initialized_m = false;
    
    // return frag descriptor to free list
    if (usethreads()) {
        gmState.sendFragList.returnElement(this, 0);
    } else {
        gmState.sendFragList.returnElementNoLock(this, 0);
    }
}

