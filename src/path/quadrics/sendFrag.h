/*
 * Copyright 2002.  The Regents of the University of California. This material 
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



#ifndef QUADRICS_SENDFRAG
#define QUADRICS_SENDFRAG

#ifdef __linux__
#include <endian.h>
#endif

#include <elan3/elan3.h>

#include "queue/globals.h"             // needed for reliabilityInfo...
#include "util/DblLinkList.h"        // needed for DoubleLinkList
#include "path/common/BaseDesc.h"        // needed for BaseSendDesc_t
#include "path/quadrics/header.h"
#include "path/quadrics/state.h"
#include "os/atomic.h"

#define QSF_INFO_INITIALIZED 0x1
#define QSF_INITONCE_COMPLETE 0x2
#define QSF_INIT_COMPLETE 0x4
#define QSF_DMA_ENQUEUED 0x8

/* process-private send frag descriptors, could be stored in array of lists indexed by rail index */

class quadricsSendFragDesc : public Links_t {
public:

    // default constructor
    quadricsSendFragDesc(int poolIndex = 0) {
    }

    // default destructor
    virtual ~quadricsSendFragDesc() {
        if (srcEnvelope) {
            quadricsHdrs.free(srcEnvelope);
            srcEnvelope = 0;
        }
        freeEachSendResources();
        freeInitOnceResources();
        flags = 0;
    }

    BaseSendDesc_t *parentSendDesc;	//!< pointer to "owning" message descriptor
    ELAN3_CTX *ctx;				//!< pointer to Quadrics context for DMA...
    int cmType;				//!< type of control message (data, mem. req., mem. release)
    int flags;				//!< state of this frag (see QSF_* defines)
    int rail;				//!< Quadrics rail index of ctx
    int nEachSendRscs;			//!< number of "each send" resources have been alloc.
    int nDMADescsNeeded;			//!< number of DMA descs. needed to send this frag
    int numTransmits;			//!< number of times that we have attempted transmission
    int globalDestID;			//!< global destination process ID
    int tmap_index;				//!< typemap index for datatypes
    bool usePackBuffer;			//!< need to pack data, or not...
    bool freeWhenDone;			//!< free or possibly retransmit when done...
    unsigned char *srcAddr;			//!< base pointer to user application data
    void *destAddr;				//!< pointer to remote data buffer
    int destBufType;			//!< for peerMemoryTracker.push()
    size_t sequential_offset;		//!< "contiguous" byte offset as if all data was packed
    size_t fragLength;			//!< frag data length in bytes
    double timeSent;			//!< time that the frag was enqueued for DMA...
    unsigned long long frag_seq;		//!< duplicate/dropped detection sequence number
    quadricsCtlHdr_t *srcEnvelope;		//!< not necessarily aligned memory to store frag envelope info
    //   before init

    /* set up by init() once (context dependent) */
    quadricsCtlHdr_t *fragEnvelope;		//!< ptr to 64-byte aligned memory for frag envelope info
    E3_Addr elanFragEnvelope;		//!< Elan address of fragEnvelope
    sdramaddr_t elanEnvelope;		//!< SDRAM frag envelope info copy (for performance)
    E3_Addr elanElanEnvelope;		//!< Elan address of elanEnvelope
    E3_DMA_MAIN *mainDMADesc;		//!< array of ptrs to Elan-addr. 64-byte algn. main memory DMA descs.

    /* allocated every send, if needed (context dependent) */
    unsigned char *packedData;		//!< ptr to Elan-addr. 64-byte aligned main memory for frag data
    sdramaddr_t srcEvent[3];		//!< E3_Event memory on Elan
    E3_Addr elanSrcEvent[3];
    sdramaddr_t elanDMADesc[3];		//!< E3_DMA memory on Elan
    volatile E3_Event_Blk *srcEventBlk;		//!< obtained via dmaThrottle()

    // called each time this descriptor is removed from the free list
    // ...should be called until true is returned...
    bool init(BaseSendDesc_t *message = 0,
              ELAN3_CTX *context = 0,
              enum quadricsCtlMsgTypes cmtype = MESSAGE_DATA,
              int railnum = 0,
              int destID = 0,
              int dtypeIndex = 0,
              unsigned char *addr = 0,
              size_t soffset = 0,
              size_t flen = 0,
              void *dstaddr = 0,
              int dstBufType = 0,
              bool needsPacking = false,
              void *srcenv = 0)
        {
            bool result = true;

            if ((flags & QSF_INFO_INITIALIZED) == 0) {

                // free context/rail dependent resources if not the same context/rail
                if ((rail != railnum) || (ctx != context)) {
                    freeInitOnceResources();
                }

                /* initialize frag information */
                parentSendDesc = message;
                ctx = context;
                cmType = cmtype;
                rail = railnum;
                numTransmits = 0;
                globalDestID = destID;
                tmap_index = dtypeIndex;
                usePackBuffer = needsPacking;
                freeWhenDone = false;
                sequential_offset = soffset;
                fragLength = flen;
                timeSent = -1.0;
                frag_seq = 0;
                srcAddr = addr;
                destAddr = dstaddr;
                destBufType = dstBufType;
                srcEnvelope = (quadricsCtlHdr_t *)srcenv;

                flags |= QSF_INFO_INITIALIZED;
            }

            /* calculate how many DMA descriptors are needed */
            if (fragLength <= CTLHDR_DATABYTES) {
                nDMADescsNeeded = 1;
            }
            else {
                /* get buffer to pack data if needed */
                if (usePackBuffer ||
                    (elan3_addressable(ctx,(srcAddr + sequential_offset), fragLength) == FALSE)) {
                    if (!packedData)
                        packedData = (unsigned char *)elan3_allocMain(ctx, E3_BLK_ALIGN, fragLength);
                    if (!packedData) {
                        return false;
                    }
                    usePackBuffer = true;
                }
                /* Does the source buffer start within 64 bytes of the end of a
                 * page, and does the DMA straddle into the next page? If so,
                 * then we use an additional DMA to avoid an Elan3 bug... */
                unsigned long mask = ~(ctx->pageSize - 1);
                unsigned char *buf = (packedData) ? (packedData) : (srcAddr + sequential_offset);
                if ( (((unsigned long)buf & mask) != ((unsigned long)(buf + 64) & mask)) &&
                     (((unsigned long)buf & mask) != ((unsigned long)(buf + fragLength) & mask)) ) {
                    nDMADescsNeeded = 3;
                }
                else {
                    nDMADescsNeeded = 2;
                }
            }


            /* make sure we get enough resources to send this frag */
            if ((flags & QSF_INITONCE_COMPLETE) == 0) {
                result = initInitOnceResources();
            }

            if (result) {
                result = initEachSendResources();
            }

            /* initialize DMA/Event structures, pack data, create envelope information,
             * calculate checksums, and get frag_seq as needed
             */
            if (result) {
#ifdef RELIABILITY_ON
                if ((cmType == MESSAGE_DATA) && (frag_seq == 0))
                    initFragSeq();
#endif
                switch (nDMADescsNeeded) {
                case 1:
                    initEnvelope(0, -1);
                    break;
                case 2:
                    initData(0, false);
                    initEnvelope(1, 0);
                    break;
                case 3:
                    initData(0, true);
                    initEnvelope(2, 1);
                    break;
                }

                /* mark descriptor initialization as done */
                flags |= QSF_INIT_COMPLETE;
            }

            return result;
        }

    // only called if new context and rail needed to resend...
    bool reinit(ELAN3_CTX *context, int railnum, void *destMemAddr)
        {
            bool needDestMemory = (destAddr) ? true : false;
            // save destination memory address...
            if (destAddr) {
                if (quadricsDoAck) {
                    quadricsPeerMemory[rail]->push(globalDestID, destBufType, destAddr);
                }
                destAddr = 0;
            }
            // abandon Elan resources and srcEventBlk (which could be copied into later)
            discardEachSendResources();
            discardInitOnceResources();
            // set new rail and context
            rail = railnum;
            ctx = context;
            // new destination memory address
            if (needDestMemory) {
                destAddr = destMemAddr;
            }
            // reset flags to info initialized state
            flags = QSF_INFO_INITIALIZED;
            // call init again...
            return init();
        }

    // called before this descriptor is returned to its free list
    void freeRscs()
        {
            if (destAddr) {
                /* reuse destination buffer only if we are doing ACKs */
                if (quadricsDoAck) {
                    quadricsPeerMemory[rail]->push(globalDestID, destBufType, destAddr);
                }
                destAddr = 0;
            }
            if (srcEnvelope) {
                quadricsHdrs.free(srcEnvelope);
                srcEnvelope = 0;
            }
            /*
              freeEachSendResources();
            */
            if (packedData) {
                elan3_free(ctx, packedData);
                packedData = 0;
            }
            if (srcEventBlk) {
                quadricsThrottle->freeEventBlk(rail, srcEventBlk, &srcEventBlk);
                srcEventBlk = 0;
            }
            flags = QSF_INITONCE_COMPLETE;
        }

    // wrapper around elan3_putdma() to allow path timeout check...
    bool enqueue(double timeNow, int *errorCode)
        {
            *errorCode = ULM_SUCCESS;

#ifdef RELIABILITY_ON
            if (flags & QSF_DMA_ENQUEUED) {
                if ((srcEventBlk == 0) ||
                    quadricsThrottle->eventBlkReady(rail, srcEventBlk, &srcEventBlk)) {
                    // if "done" then reinitialize and prime the events and queue DMA
                    return reinitAndResend(errorCode);
                } else {
                    // if not "done" then return false immediately
                    // check to see if this rail is bad (DMA does not
                    // complete within QUADRICS_BADRAIL_DMA_TIMEOUT
                    // seconds)
                    if ((timeNow - timeSent) >= QUADRICS_BADRAIL_DMA_TIMEOUT) {
                        *errorCode = ULM_ERR_BAD_SUBPATH;
                    }
                    return false;
                }
            }
            else {
#endif
                /* this memory barrier should be unnecessary, if handled by elan3_putdma() */
                mb();

                elan3_putdma(ctx, elanDMADesc[0]);
                flags |= QSF_DMA_ENQUEUED;
                return true;
#ifdef RELIABILITY_ON
            }
#endif
        }

    bool done(double timeNow, int *errorCode)
        {
            *errorCode = ULM_SUCCESS;

            if (flags & QSF_DMA_ENQUEUED) {
#ifdef RELIABILITY_ON
                if ((srcEventBlk != 0) &&
                    quadricsThrottle->eventBlkReady(rail, srcEventBlk, &srcEventBlk)) {
                    return true;
                }
                else {
                    if ((timeNow - timeSent) >= QUADRICS_BADRAIL_DMA_TIMEOUT) {
                        if (freeWhenDone) {
                            if (destAddr) {
                                if (quadricsDoAck) {
                                    quadricsPeerMemory[rail]->
                                        push(globalDestID, destBufType, destAddr);
                                }
                                destAddr = 0;
                            }
                            discardEachSendResources();
                            discardInitOnceResources();
                            return true;
                        }
                        else {
                            *errorCode = ULM_ERR_BAD_SUBPATH;
                        }
                    }
                    return false;
                }
#else
                return quadricsThrottle->eventBlkReady(rail, srcEventBlk, &srcEventBlk);
#endif
            }
            return (freeWhenDone) ? true : false;
        }

    // do a quick checksum of the quadricsCtlHdr_t
    unsigned int csumCtlHdr()
        {
            unsigned int csum, i;
#if BYTE_ORDER == LITTLE_ENDIAN
            unsigned char *tmpp, *csump;
            unsigned int tmp;
#endif

            if (usecrc()) {
                csum = uicrc(fragEnvelope, sizeof(quadricsCtlHdr_t) - sizeof(ulm_uint32_t));
#if BYTE_ORDER == LITTLE_ENDIAN
                /* byte swap if little endian - so CRC of header + CRC yields 0 */
                csump = (unsigned char *)&csum;
                tmpp = (unsigned char *)&tmp;
                tmpp[0] = csump[3];
                tmpp[1] = csump[2];
                tmpp[2] = csump[1];
                tmpp[3] = csump[0];
                csum = tmp;
#endif
            }
            else {
                csum = 0;
                for (i = 0; i < CTLHDR_WORDS; i++) {
                    csum += fragEnvelope->words[i];
                }
            }
            return csum;
        }

private:

    // sets up the quadricsCtlHdr_t to be sent, and chains the envelope QDMA to
    // another DMA/Event pair if chainedIndex >= 0...
    inline void initEnvelope(int index, int chainedIndex);

    // initializes all the DMA/Event pair(s) needed (2 if elanbug is true, 1 otherwise)
    // starting with the DMA desc./Event pair at index
    inline void initData(int index, bool elanbug);

    // return checksum of user data copied to dest
    inline unsigned int packBuffer(void *dest);

#ifdef RELIABILITY_ON
    void initFragSeq()
        {
            /* the first frag of a synchronous send needs a valid frag_seq
             * if RELIABILITY_ON is defined -- since it is recorded and ACK'ed...
             */
            if (!quadricsDoAck) {
                if ((parentSendDesc->sendType != ULM_SEND_SYNCHRONOUS) || 
                    (sequential_offset != 0)) {
                    frag_seq = 0;
                    return;
                }
            }

            if (parentSendDesc->sendType == ULM_SEND_MULTICAST) {
                // currently, zero out - no reliability.
                frag_seq = 0;
            } else {
                // thread-safe allocation of frag sequence number in header
                if (usethreads())
                    reliabilityInfo->next_frag_seqsLock[globalDestID].lock();
                frag_seq = (reliabilityInfo->next_frag_seqs[globalDestID])++;
                if (usethreads())
                    reliabilityInfo->next_frag_seqsLock[globalDestID].unlock();
            }
        }
#endif

    bool initInitOnceResources() {
        if (!mainDMADesc) {
            mainDMADesc = (E3_DMA_MAIN *)elan3_allocMain(ctx, E3_DMA_ALIGN, sizeof(E3_DMA_MAIN));
        }
        if (!fragEnvelope) {
	    unsigned long mask = ~(ctx->pageSize - 1);
	    void *bad_addresses[10];
	    int bad_addrcnt = 0;
	    for (int i = 0; i < 10; i++) {
            	fragEnvelope = (quadricsCtlHdr_t *)elan3_allocMain(ctx, E3_BLK_ALIGN,
                                                               sizeof(quadricsCtlHdr_t));
            	if (fragEnvelope) {
			if (((unsigned long)fragEnvelope & mask) !=
				((unsigned long)((unsigned char *)fragEnvelope + 64) & mask)) {
				bad_addresses[i] = fragEnvelope;
				bad_addrcnt++;
				fragEnvelope = 0;
			}
			else {
                		elanFragEnvelope = elan3_main2elan(ctx, fragEnvelope);
				break;
			}
		}
	    }
	    if (bad_addrcnt) {
			if (bad_addrcnt == 10) {
				ulm_err(("quadricsSendFragDesc::initInitOnceResources: exceeded 10"
				" memory allocations tries for fragEnvelope!\n"));
				freeInitOnceResources();
				return false;
			}
			else {
				for (int i = 0; i < bad_addrcnt; i++) {
					elan3_free(ctx, bad_addresses[i]);
				}
			}
	    }
        }
        /* PERFORMANCE is worse!
           if (elanEnvelope == (sdramaddr_t)NULL) {
           elanEnvelope = elan3_allocElan(ctx, E3_BLK_ALIGN, sizeof(quadricsCtlHdr_t));
           if (elanEnvelope != (sdramaddr_t)NULL) {
           elanElanEnvelope = elan3_sdram2elan(ctx, ctx->sdram, elanEnvelope);
           }
           }
        */
        if (!mainDMADesc || !fragEnvelope) {
            /* free everything to avoid partial sends from
             * holding enough Elan-addressable main memory
             * resources to prevent any frag from being
             * sent...
             */
            freeInitOnceResources();
            return false;
        }
        flags |= QSF_INITONCE_COMPLETE;
        return true;
    }

    void freeInitOnceResources() {
        if (mainDMADesc) {
            elan3_free(ctx, mainDMADesc);
            mainDMADesc = 0;
        }
        if (fragEnvelope) {
            elan3_free(ctx, fragEnvelope);
            fragEnvelope = 0;
        }
        if (elanEnvelope != (sdramaddr_t)NULL) {
            elan3_freeElan(ctx, elanEnvelope);
            elanEnvelope = (sdramaddr_t)NULL;
        }
        flags &= ~QSF_INITONCE_COMPLETE;
    }

    void discardInitOnceResources() {
        if (mainDMADesc) {
            mainDMADesc = 0;
        }
        if (fragEnvelope) {
            fragEnvelope = 0;
        }
        if (elanEnvelope != (sdramaddr_t)NULL) {
            elanEnvelope = (sdramaddr_t)NULL;
        }
        flags &= ~QSF_INITONCE_COMPLETE;
    }

    bool initEachSendResources() {
        for (int i = nEachSendRscs; i < nDMADescsNeeded; i++) {
            // an E3_BlockCopyEvent is larger than an E3_Event...go figure!!
            srcEvent[i] = elan3_allocElan(ctx, E3_EVENT_ALIGN, sizeof(E3_BlockCopyEvent));
            if (srcEvent[i] != (sdramaddr_t)NULL) {
                elanDMADesc[i] = elan3_allocElan(ctx, E3_DMA_ALIGN, sizeof(E3_DMA));
                elanSrcEvent[i] = elan3_sdram2elan(ctx, ctx->sdram, srcEvent[i]);
            } else {
                elanDMADesc[i] = (sdramaddr_t)NULL;
            }
            nEachSendRscs += ((srcEvent[i] != (sdramaddr_t)NULL) &&
                              (elanDMADesc[i] != (sdramaddr_t)NULL)) ? 1 : 0;
        }
        if (!srcEventBlk) {
            srcEventBlk = quadricsThrottle->getEventBlk(rail, &srcEventBlk);
        }
        if ((nEachSendRscs < nDMADescsNeeded) || !srcEventBlk) {
            /* free everything to avoid partial sends from
             * holding enough Elan memory resources to
             * prevent any frag from being sent...
             */
            freeEachSendResources();
        }
        return (nEachSendRscs >= nDMADescsNeeded);
    }

    void freeEachSendResources() {
        for (int i = 0; i < nEachSendRscs; i++) {
            if (srcEvent[i] != (sdramaddr_t)NULL) {
                elan3_freeElan(ctx, srcEvent[i]);
            }
            if (elanDMADesc[i] != (sdramaddr_t)NULL) {
                elan3_freeElan(ctx, elanDMADesc[i]);
            }
        }
        nEachSendRscs = 0;
        if (packedData) {
            elan3_free(ctx, packedData);
            packedData = 0;
        }
        if (srcEventBlk) {
            quadricsThrottle->freeEventBlk(rail, srcEventBlk, &srcEventBlk);
            srcEventBlk = 0;
        }
    }

    void discardEachSendResources(bool keepPackedData = true) {
        for (int i = 0; i < nEachSendRscs; i++) {
            srcEvent[i] = (sdramaddr_t)NULL;
            elanDMADesc[i] = (sdramaddr_t)NULL;
        }
        nEachSendRscs = 0;
        if (packedData && !keepPackedData) {
            elan3_free(ctx, packedData);
            packedData = 0;
        }
        srcEventBlk = 0;
    }


    bool reinitAndResend(int *errorCode) {

        *errorCode = ULM_SUCCESS;

        // free the done/ready event block
        if (srcEventBlk) {
            quadricsThrottle->freeEventBlk(rail, srcEventBlk, &srcEventBlk);
            srcEventBlk = 0;
        }

        // get a new event block from the DMA throttler
        srcEventBlk = quadricsThrottle->getEventBlk(rail, &srcEventBlk);

        // if we don't get it, then return without sending
        if (!srcEventBlk)
            return false;


        // reinitialize and reprime the appropriate events
        switch (nDMADescsNeeded) {
        case 1:
            elan3_initevent_main(ctx, srcEvent[0], quadricsQueue[rail].doneBlk,
                                 (E3_Event_Blk *)srcEventBlk);
            elan3_primeevent(ctx, srcEvent[0], 1);
            break;
        case 2:
            elan3_initevent(ctx, srcEvent[0]);
            elan3_initevent_main(ctx, srcEvent[1], quadricsQueue[rail].doneBlk,
                                 (E3_Event_Blk *)srcEventBlk);
            elan3_primeevent(ctx, srcEvent[1], 1);
            elan3_waitdmaevent(ctx, elanDMADesc[1], srcEvent[0]);
            break;
        case 3:
            elan3_initevent(ctx, srcEvent[0]);
            elan3_initevent(ctx, srcEvent[1]);
            elan3_waitdmaevent(ctx, elanDMADesc[1], srcEvent[0]);
            elan3_initevent_main(ctx, srcEvent[2], quadricsQueue[rail].doneBlk,
                                 (E3_Event_Blk *)srcEventBlk);
            elan3_primeevent(ctx, srcEvent[2], 1);
            elan3_waitdmaevent(ctx, elanDMADesc[2], srcEvent[1]);
            break;
        }

        /* this memory barrier should be unnecessary, if handled by elan3_putdma() */
        mb();

        // enqueue the first DMA descriptor
        elan3_putdma(ctx, elanDMADesc[0]);
        return true;
    }
};

#include "client/ULMClient.h"
#include <string.h>

inline void quadricsSendFragDesc::initEnvelope(int index, int chainedIndex)
{
    E3_DMA_MAIN *qdma = mainDMADesc;

    /* initialize the QDMA descriptor */
    qdma->dma_u.type = E3_DMA_TYPE(DMA_BYTE, DMA_WRITE, DMA_QUEUED, 63);
    qdma->dma_dest = 0;
    qdma->dma_destEvent = quadricsQueue[rail].elanQAddr;	// global queue object
    qdma->dma_size = sizeof(quadricsCtlHdr_t);
    qdma->dma_source = (elanEnvelope != (sdramaddr_t)NULL) ? elanElanEnvelope : elanFragEnvelope;
    qdma->dma_srcEvent = elanSrcEvent[index];
    qdma->dma_srcCookieProc.cookie_vproc = elan3_local_cookie(ctx, myproc(), globalDestID); // Quadrics VPIDs
    qdma->dma_destCookieProc.cookie_vproc = globalDestID;

    elan3_copy32_to_sdram(ctx->sdram, qdma, elanDMADesc[index]);

    /* initialize the corresponding event as a block copy event */

    elan3_initevent_main(ctx, srcEvent[index], quadricsQueue[rail].doneBlk, (E3_Event_Blk *)srcEventBlk); // global "done" block

    /* now prime the event so that it goes off */

    elan3_primeevent(ctx, srcEvent[index], 1);

    /* initialize the quadricsCtlHdr_t with info that we have */

    if (srcEnvelope) {
        size_t bytesToCopy;
        switch (cmType) {
        case MESSAGE_DATA_ACK:
            bytesToCopy = sizeof(quadricsDataAck_t);
            break;
        case MEMORY_RELEASE:
            bytesToCopy = sizeof(quadricsMemRls_t);
            break;
        case MEMORY_REQUEST:
            bytesToCopy = sizeof(quadricsMemReq_t);
            break;
        case MEMORY_REQUEST_ACK:
            bytesToCopy = sizeof(quadricsMemReqAck_t);
            break;
        default:
            bytesToCopy = sizeof(quadricsCtlHdr_t);
            break;
        }
        memcpy(fragEnvelope, srcEnvelope, bytesToCopy);
        quadricsHdrs.free(srcEnvelope);
        srcEnvelope = 0;
    }

    // overwrite fragEnvelope->commonHdr values
    fragEnvelope->commonHdr.ctlMsgType = (unsigned short)cmType;
    fragEnvelope->commonHdr.checksum = 0;

    /* sender side control message types */

    switch (cmType) {
    case MESSAGE_DATA:
    {
        quadricsDataHdr_t *p = &(fragEnvelope->msgDataHdr);
        if (parentSendDesc->sendType == ULM_SEND_SYNCHRONOUS) {
            p->ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE(parentSendDesc->ctx_m,
			    MSGTYPE_PT2PT_SYNC);
        }
        else {
            p->ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE(parentSendDesc->ctx_m,
			    MSGTYPE_PT2PT);
        }
        p->tag_m = parentSendDesc->tag_m;
        p->senderID = myproc();
        p->destID = globalDestID;
        p->msgLength = parentSendDesc->PostedLength;
        p->frag_seq = frag_seq;
        p->isendSeq_m = parentSendDesc->isendSeq_m;
        p->sendFragDescPtr.ptr = this;
        p->dataSeqOffset = sequential_offset;
        p->dataLength = fragLength;
        // we send E3_Addr 32-bit addresses
        p->dataElanAddr = (ulm_uint32_t)destAddr;
    }
    break;
    case MESSAGE_DATA_ACK:
    {
        quadricsDataAck_t *p = &(fragEnvelope->msgDataAck);
        p->senderID = myproc();
        p->destID = globalDestID;
    }
    break;
    case MEMORY_RELEASE:
    {
        quadricsMemRls_t *p = &(fragEnvelope->memRelease);
        p->senderID = myproc();
        p->destID = globalDestID;
    }
    break;
    case MEMORY_REQUEST:
    {
        quadricsMemReq_t *p = &(fragEnvelope->memRequest);
        p->msgLength = parentSendDesc->PostedLength;
        p->sendMessagePtr.ptr = parentSendDesc;
        p->tag_m = parentSendDesc->tag_m;
        p->senderID = myproc();
        p->destID = globalDestID;
        p->ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE(parentSendDesc->ctx_m,
                                                    MSGTYPE_PT2PT);
    }
    break;
    case MEMORY_REQUEST_ACK:
    {
        quadricsMemReqAck_t *p = &(fragEnvelope->memRequestAck);
        p->senderID = myproc();
        p->destID = globalDestID;
    }
    break;
    }

    /* copy any data and checksum control message header if this is a data frag */

    if ((cmType == MESSAGE_DATA) && fragLength) {
        if (fragLength <= CTLHDR_DATABYTES) {
            if (usePackBuffer) {
                /* non-contiguous data - checksum free */
                fragEnvelope->msgDataHdr.dataChecksum =
                    packBuffer(fragEnvelope->msgDataHdr.data);
            }
            else {
                if (usecrc()) {
                    /* copy contiguous data into fragEnvelope - CRC relatively expensive */
                    fragEnvelope->msgDataHdr.dataChecksum = bcopy_uicrc((srcAddr + sequential_offset),
                                                                        fragEnvelope->msgDataHdr.data, fragLength, fragLength);
                }
                else {
                    /* copy contiguous data into fragEnvelope - checksum almost free */
                    fragEnvelope->msgDataHdr.dataChecksum = bcopy_uicsum((srcAddr + sequential_offset),
                                                                         fragEnvelope->msgDataHdr.data, fragLength, fragLength);
                }
            }
        }
        else {
            if (usePackBuffer) {
                /* contiguous data in non-elan addressable memory or non-contiguous data
                 * - checksum almost free */
                fragEnvelope->msgDataHdr.dataChecksum =
                    packBuffer(packedData);
            }
            else if (quadricsDoChecksum) {
                if (usecrc()) {
                    /* calculate CRC for contiguous data which is already Elan addressable */
                    fragEnvelope->msgDataHdr.dataChecksum = uicrc((srcAddr + sequential_offset),
                                                                  fragLength);
                }
                else {
                    /* checksum contiguous data which is already Elan addressable */
                    fragEnvelope->msgDataHdr.dataChecksum = uicsum((srcAddr + sequential_offset),
                                                                   fragLength);
                }
            }
        }
    }

    /* calculate the control message header checksum */
    if (quadricsDoChecksum)
        fragEnvelope->commonHdr.checksum = csumCtlHdr();

    /* copy the QDMA payload to SDRAM for performance, if allocated successfully */
    if (elanEnvelope != (sdramaddr_t)NULL) {
        elan3_blkcopy64_to_sdram(ctx->sdram, fragEnvelope, elanEnvelope, sizeof(quadricsCtlHdr_t));
    }

    /* chain this QDMA descriptor off of the event at chainedIndex, if valid index */
    if (chainedIndex >= 0) {
        elan3_waitdmaevent(ctx, elanDMADesc[index], srcEvent[chainedIndex]);
    }

    return;
}

inline void quadricsSendFragDesc::initData(int index, bool elanbug)
{
    E3_DMA_MAIN *d = mainDMADesc;
    int toEndOfPageBytes = 0;

    /* calculate toEndOfPageBytes, if needed */

    if (elanbug) {
        unsigned long mask = (ctx->pageSize - 1);
        if (usePackBuffer) {
            toEndOfPageBytes = ctx->pageSize - (((unsigned long)packedData) & mask);
        }
        else {
            toEndOfPageBytes = ctx->pageSize - (((unsigned long)srcAddr + sequential_offset) & mask);
        }
    }

    /* initialize the first DMA descriptor */
    d->dma_u.type = E3_DMA_TYPE(DMA_BYTE, DMA_WRITE, DMA_NORMAL, 63);
    d->dma_dest = (E3_Addr)destAddr;
    d->dma_destEvent = (E3_Addr)0;
    d->dma_size = (elanbug) ? toEndOfPageBytes : fragLength;
    d->dma_source = elan3_main2elan(ctx, (usePackBuffer) ? packedData : (srcAddr + sequential_offset));
    d->dma_srcEvent = elanSrcEvent[index];
    d->dma_srcCookieProc.cookie_vproc = elan3_local_cookie(ctx, myproc(), globalDestID); // Quadrics VPIDs
    d->dma_destCookieProc.cookie_vproc = globalDestID;

    elan3_copy32_to_sdram(ctx->sdram, d, elanDMADesc[index]);

    /* initialize the corresponding event */

    elan3_initevent(ctx, srcEvent[index]);

    if (elanbug) {
        /* initialize the second DMA descriptor */
	    d->dma_u.type = E3_DMA_TYPE(DMA_BYTE, DMA_WRITE, DMA_NORMAL, 63);

        d->dma_dest = (E3_Addr)((unsigned char *)destAddr + toEndOfPageBytes);
        d->dma_destEvent = (E3_Addr)0;
        d->dma_size = fragLength - toEndOfPageBytes;
        d->dma_source = elan3_main2elan(ctx, (usePackBuffer) ? (packedData + toEndOfPageBytes) :
                                        (srcAddr + sequential_offset + toEndOfPageBytes));
        d->dma_srcEvent = elanSrcEvent[index + 1];
        d->dma_srcCookieProc.cookie_vproc = elan3_local_cookie(ctx, myproc(), globalDestID); // Quadrics VPIDs
        d->dma_destCookieProc.cookie_vproc = globalDestID;

        elan3_copy32_to_sdram(ctx->sdram, d, elanDMADesc[index + 1]);

        /* initialize the corresponding event */

        elan3_initevent(ctx, srcEvent[index + 1]);

        /* prime and chain the second DMA descriptor to the first event */

        elan3_waitdmaevent(ctx, elanDMADesc[index + 1], srcEvent[index]);
    }

    return;
}

inline unsigned int quadricsSendFragDesc::packBuffer(void *dest)
{
    /* contiguous data */
    if (tmap_index < 0) {
        if (quadricsDoChecksum) {
            if (usecrc()) {
                return bcopy_uicrc((srcAddr + sequential_offset), dest, fragLength, fragLength);
            }
            else {
                return bcopy_uicsum((srcAddr + sequential_offset), dest, fragLength, fragLength);
            }
        }
        else {
            MEMCOPY_FUNC((srcAddr + sequential_offset), dest, fragLength);
            return 0;
        }
    }

    /* non-contiguous data */
    unsigned char *src_addr, *dest_addr = (unsigned char *)dest;
    size_t len_to_copy, len_copied;
    ULMType_t *dtype = parentSendDesc->datatype;
    ULMTypeMapElt_t *tmap = dtype->type_map;
    int dtype_cnt, ti;
    int tm_init = tmap_index;
    int init_cnt = sequential_offset / dtype->packed_size;
    int tot_cnt = parentSendDesc->PostedLength / dtype->packed_size;
    unsigned char *start_addr = srcAddr + init_cnt * dtype->extent;
    unsigned int csum = 0, ui1 = 0, ui2 = 0;


    // handle first typemap pair
    src_addr = start_addr
        + tmap[tm_init].offset
        - init_cnt * dtype->packed_size - tmap[tm_init].seq_offset + sequential_offset;
    len_to_copy = tmap[tm_init].size
        + init_cnt * dtype->packed_size + tmap[tm_init].seq_offset - sequential_offset;
    len_to_copy = (len_to_copy > fragLength) ? fragLength : len_to_copy;

    if (quadricsDoChecksum) {
        if (usecrc()) {
            csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, CRC_INITIAL_REGISTER);
        }
        else {
            csum = bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
        }
    }
    else {
        MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
    }
    len_copied = len_to_copy;

    tm_init++;
    for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
        for (ti = tm_init; ti < dtype->num_pairs; ti++) {
            src_addr = start_addr + tmap[ti].offset;
            dest_addr = (unsigned char *)dest + len_copied;
            len_to_copy = (fragLength - len_copied >= tmap[ti].size) ?
                tmap[ti].size : fragLength - len_copied;
            if (len_to_copy == 0) {
                return csum;
            }

            if (quadricsDoChecksum) {
                if (usecrc()) {
                    csum = bcopy_uicrc(src_addr, dest_addr, len_to_copy, len_to_copy, csum);
                }
                else {
                    csum += bcopy_uicsum(src_addr, dest_addr, len_to_copy, len_to_copy, &ui1, &ui2);
                }
            }
            else {
                MEMCOPY_FUNC(src_addr, dest_addr, len_to_copy);
            }
            len_copied += len_to_copy;
        }

        tm_init = 0;
        start_addr += dtype->extent;
    }

    return csum;
}

#endif
