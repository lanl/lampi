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

#define INSTANTIATE_QUADRICS_GLOBALS

#include <sys/errno.h>
#include <unistd.h>
#include <rms/rmscall.h>

#include "client/ULMClient.h"
#include "queue/globals.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "path/quadrics/state.h"
#include "path/quadrics/recvFrag.h"
#include "path/quadrics/sendFrag.h"
#include "ulm/ulm.h"

/* initialization code */

void quadricsInitRecvDescs() {
    long minPagesPerList = 4;
    long maxPagesPerList = -1;
    long nPagesPerList = 4;

    ssize_t pageSize = PAGESIZE;
    ssize_t elementSize = sizeof(quadricsRecvFragDesc);
    ssize_t poolChunkSize = PAGESIZE;
    int nFreeLists = local_nprocs();

    bool useInputPool = true;
    bool enforceMemoryPolicy = true;
    int *memAffinityPool = (int *)ulm_malloc(sizeof(int) * nFreeLists);
    if (!memAffinityPool) {
	ulm_exit((-1, "quadricsInitRecvDescs: Unable to allocate space "
                  "for memAffinityPool\n"));
    }

    // fill in memory affinity index
    for(int i = 0 ; i < nFreeLists ; i++ ) {
	memAffinityPool[i] = i;
    }

    int retVal = quadricsRecvFragDescs.Init
	( nFreeLists, nPagesPerList, poolChunkSize, pageSize, elementSize,
	  minPagesPerList, maxPagesPerList, maxQuadricsRecvFragsDescRetries,
	  " Quadrics recv frag descriptors ",
	  quadricsRecvFragsDescRetryForResources,
	  memAffinityPool, enforceMemoryPolicy,
	  ShareMemDescPool, quadricsRecvFragsDescAbortWhenNoResource
	    );

    if ( retVal ) {
	ulm_err(("Unable to initialize quadricsRecvFragDescs freelists!\n"));
	exit(1);
    }

    return;
}

// don't enforceMemoryPolicy since Quadrics support
// is for platforms on which memory locality support
// is limited to nonexistent...more important to use
// each list to separate descriptors on a per rail
// basis -- to eliminate reallocating "initOnce"
// resources...
void quadricsInitSendFragDescs() {
    long minPagesPerList = 4;
    long maxPagesPerList = -1;
    long nPagesPerList = 4;

    ssize_t pageSize = PAGESIZE;
    ssize_t elementSize = sizeof(quadricsSendFragDesc);
    ssize_t poolChunkSize = PAGESIZE;
    int nFreeLists = quadricsNRails;

    bool useInputPool = false;
    bool enforceMemoryPolicy = false;

    int retVal = quadricsSendFragDescs.Init
	( nFreeLists, nPagesPerList, poolChunkSize, pageSize, elementSize,
	  minPagesPerList, maxPagesPerList, maxQuadricsSendFragsDescRetries,
	  " Quadrics send frag descriptors ",
	  quadricsSendFragDescsDescRetryForResources,
	  0, enforceMemoryPolicy,
	  0, quadricsSendFragDescsDescAbortWhenNoResource);

    if ( retVal ) {
        ulm_err(("Unable to initialize quadricsSendFragDescs freelists!\n"));
        exit(1);
    }

    return;
}

ELAN3_CTX *getElan3Ctx(int rail) {
    return quadricsQueue[rail].ctx;
}

#ifndef ELAN_ALIGNUP
#define ELAN_ALIGNUP(x,a)       (((uintptr_t)(x) + ((a)-1)) & (-(a))) /* 'a' power of 2 */
#endif

#ifndef offsetof
#define offsetof(T,F)   ((int)&(((T *)0)->F))
#endif

void quadricsInitQueueInfo() {

    int pageSize = sysconf(_SC_PAGESIZE);
    E3_uint32 msize = ELAN_ALIGNUP(ELAN_ALLOC_SIZE, pageSize);
    E3_uint32 esize = ELAN_ALIGNUP(ELAN_ALLOCELAN_SIZE, pageSize);
    E3_Addr mbase = (E3_Addr)ELAN_ALLOC_BASE;
    E3_Addr ebase = (E3_Addr)ELAN_ALLOCELAN_BASE;

    int *dev = (int *)ulm_malloc(sizeof(int) * quadricsNRails);
    if (!dev) {
        ulm_err(("quadricsInitQueueInfo: malloc for %d ints for dev failed!\n",
                 quadricsNRails));
        exit(1);
    }

    if (quadricsNRails != elan3_rails(&quadricsCap, dev)) {
        ulm_err(("quadricsInitQueueInfo: elan3_rails does not agree with elan3_nrails!\n"));
        exit(1);
    }

    quadricsQueue = (quadricsQueueInfo_t *)ulm_malloc(sizeof(quadricsQueueInfo_t) * quadricsNRails);
    if (!quadricsQueue) {
        ulm_err(("quadricsInitQueueInfo: quadricsQueue allocation of %d bytes failed!\n",
                 sizeof(quadricsQueueInfo_t) * quadricsNRails));
        exit(1);
    }

    for (int i = 0; i < quadricsNRails; i++) {

        quadricsQueueInfo_t *p = &(quadricsQueue[i]);

        new (p) quadricsQueueInfo_t;
        p->railOK = true;
        p->ctlMsgsToSendFlag = p->ctlMsgsToAckFlag = 0;

        // initalize the Elan3 device
        p->ctx = elan3_init(dev[i], mbase, msize, ebase, esize);
        if (p->ctx == (ELAN3_CTX *)NULL) {
            ulm_err(("quadricsInitQueueInfo: elan3_init(%d, 0x%x, %d, 0x%x, %d) failed!\n",
                     dev[i], mbase, msize, ebase, esize));
            exit(1);
        }

        if (!elan3_set_standard_mappings(p->ctx)) {
            ulm_err(("quadricsInitQueueInfo: set standard mappings failed!\n"));
            exit(1);
        }

        if (!elan3_set_required_mappings(p->ctx)) {
            ulm_err(("quadricsInitQueueInfo: set required mappings failed!\n"));
            exit(1);
        }

        // block inputter until we are ready...
        elan3_block_inputter(p->ctx, 1);

        // attach...
        int res;
        if ((res = elan3_attach(p->ctx, &quadricsCap)) != 0) {
            ulm_err(("[%d] quadricsInitQueueInfo: elan3_attach failed with error %d returned %d!\n",
                     myproc(),errno, res));
            exit(1);
        }

        // call elan3_addvp to map this process into the whole group
        if (elan3_addvp(p->ctx, 0, &quadricsCap) < 0) {
            ulm_err(("quadricsInitQueueInfo: elan3_addvp failed!\n"));
            exit(1);
        }

        // check VPID value to make sure it matches global process ID...
        if (elan3_process(p->ctx) != myproc()) {
            ulm_err(("quadricsInitQueueInfo: elan3_process %d != myproc %d\n",
                     elan3_process(p->ctx), myproc()));
            exit(1);
        }

        // allocate the E3_Queue object in Elan SDRAM -- round up to nearest block size
        // since we will actually touch q_event as a block copy event (16 bytes > 8 allocated for
        // an E3_Event)
        p->sdramQAddr = elan3_allocElan(p->ctx, E3_QUEUE_ALIGN,
                                        ELAN_ALIGNUP(sizeof(E3_Queue), E3_BLK_SIZE));
        if (p->sdramQAddr == (sdramaddr_t)NULL) {
            ulm_err(("quadricsInitQueueInfo: E3_Queue Elan memory allocation failed!\n"));
            exit(1);
        }

        // zero out the entire allocated queue structure
        elan3_zero_byte_sdram(p->ctx->sdram, p->sdramQAddr, ELAN_ALIGNUP(sizeof(E3_Queue),
                                                                         E3_BLK_SIZE));

        // allocate the "source" E3_Event_Blk in Elan SDRAM
        p->doneBlk = elan3_allocElan(p->ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk));
        if (p->doneBlk == (sdramaddr_t)NULL) {
            ulm_err(("quadricsInitQueueInfo: source E3_Event_Blk allocation failed!\n"));
            exit(1);
        }

        // allocate the "target" E3_Event_Blk in main memory for QDMA receives
        p->rcvBlk = (E3_Event_Blk *)elan3_allocMain(p->ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk));
        if (p->rcvBlk == (E3_Event_Blk *)NULL) {
            ulm_err(("quadricsInitQueueInfo: target E3_Event_Blk allocation failed!\n"));
            exit(1);
        }

        // initialize the main memory queue fields, etc.
        p->elanQAddr = elan3_sdram2elan(p->ctx, p->ctx->sdram, p->sdramQAddr);
        p->nSlots = 320; // 5 8KB pages
        p->queueSlots = (quadricsCtlHdr_t *)elan3_allocMain(p->ctx, E3_BLK_ALIGN,
                                                            sizeof(quadricsCtlHdr_t) * p->nSlots);
        if (p->queueSlots == (quadricsCtlHdr_t *)NULL) {
            ulm_err(("quadricsInitQueueInfo: queue slot main memory allocation failed!\n"));
            exit(1);
        }
        p->elanQueueSlots = elan3_main2elan(p->ctx, p->queueSlots);
        p->q_base = (caddr_t)p->queueSlots;
        p->q_top = (caddr_t)&(p->queueSlots[p->nSlots - 1]);
        p->q_fptr = p->q_base;

        // lock the queue
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_state),
                            E3_QUEUE_LOCKED);

        // initialize Elan E3_Queue fields
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_size),
                            sizeof(quadricsCtlHdr_t));
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_bptr),
                            elan3_main2elan(p->ctx, p->q_fptr));
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_fptr),
                            elan3_main2elan(p->ctx, p->q_fptr));
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_base),
                            elan3_main2elan(p->ctx, p->q_base));
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_top),
                            elan3_main2elan(p->ctx, p->q_top));

        // initialize the source and target event blocks and queue block copy event
        elan3_initevent_main (p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event),
                              p->doneBlk, (E3_Event_Blk *)p->rcvBlk);
        E3_RESET_BCOPY_BLOCK(p->rcvBlk);

        // prime the event so it fires on the first receive
        elan3_primeevent(p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event), 1);

        // unlock the queue
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_state),
                            0);
    }

    free(dev);
}

void quadricsInitBeforeFork() {
    // get capability
    if (rms_getcap(0, &quadricsCap) < 0) {
        ulm_err(("quadricsInitBeforeFork: rms_getcap(0, %p) failed!\n", &quadricsCap));
        exit(1);
    }

    // calculate quadricsNRails
    quadricsNRails = elan3_nrails(&quadricsCap);
    if (quadricsNRails <= 0) {
        ulm_err(("quadricsInitBeforeFork: elan3_nrails returned %d rails!\n", quadricsNRails));
        exit(1);
    }

    // set up process private quadricsSendFragDescs
    // note: a freelist per Quadrics rail
    quadricsInitSendFragDescs();

    // set up shared memory quadricsRecvFragDescs
    quadricsInitRecvDescs();

    // allocate and create quadricsThrottle (shared memory - writable bits...)
    quadricsThrottle = new dmaThrottle(true, true, 128, quadricsNRails);
    if (!quadricsThrottle) {
        ulm_err(("quadricsInitBeforeFork: dmaThrottle allocation failed!\n"));
        exit(1);
    }

}

void quadricsInitAfterFork() {
    int capIndex;

    if (rms_mycap(&capIndex) < 0) {
        ulm_err(("quadricsInitAfterFork: rms_mycap failed!\n"));
        exit(1);
    }

    // get and stash capability for this process - rms_setcap() has already been called...
    if (rms_getcap(capIndex, &quadricsCap) < 0) {
        ulm_err(("quadricsInitAfterFork: rms_getcap(%d, %p) failed!\n", capIndex,
                 &quadricsCap));
        exit(1);
    }

    // allocate and create quadricsPeerMemory (process private)
    quadricsPeerMemory = (peerMemoryTracker **)ulm_malloc(quadricsNRails * sizeof(peerMemoryTracker *));
    if (!quadricsPeerMemory) {
        ulm_err(("quadricsInitAfterFork: quadricsPeerMemory allocation failed!\n"));
        exit(1);
    }
    for (int i = 0; i < quadricsNRails; i++) {
        quadricsPeerMemory[i] = new peerMemoryTracker(8, true);
        if (!quadricsPeerMemory[i]) {
            ulm_err(("quadricsInitAfterFork: quadricsPeerMemory[%d] allocation failed!\n", i));
            exit(1);
        }
    }

    // create and initialize quadricsQueue[quadricsNRails] array (process private)
    quadricsInitQueueInfo();

    // allocate and create quadricsThrottle (shared memory - writable bits...)
    quadricsThrottle->postQuadricsInit();

    // create and initialize memory release structures
    quadricsMemRlsSeqs = ulm_new(long long, nprocs());
    if (!quadricsMemRlsSeqs) {
        ulm_err(("quadricsInitAfterFork: quadricsMemRlsSeqs[%d] allocation failed!\n", nprocs()));
        exit(1);
    }
    // initialize memory release sequence numbers to first valid value -- 1
    for (int i = 0; i < nprocs(); i++) {
        quadricsMemRlsSeqs[i] = 1;
    }
    quadricsMemRlsSeqList = ulm_new(SeqTrackingList, nprocs());
    if (!quadricsMemRlsSeqList) {
        ulm_err(("quadricsInitAfterFork: quadricsMemRlsSeqList[%d] allocation failed!\n", nprocs()));
        exit(1);
    }

    quadricsLastMemRls = dclock();

    // unblock the inputter on all rails now...
    for (int i = 0; i < quadricsNRails; i++) {
        elan3_block_inputter(quadricsQueue[i].ctx, 0);
    }
}
