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

#include <sys/errno.h>
#include <unistd.h>
#include <elan3/elan3.h>
#include <elan/elan.h>
#include <elan/version.h>
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

#ifndef ELAN_ALIGNUP
#define ELAN_ALIGNUP(x,a)       (((uintptr_t)(x) + ((a)-1)) & (-(a)))
/* 'a' power of 2 */
#endif

#define INSTANTIATE_QUADRICS_GLOBALS

void quadricsInitRecvDescs() {
    long minPagesPerList = 4;
    long maxPagesPerList = -1;
    long nPagesPerList = 4;

    ssize_t pageSize = PAGESIZE;
    ssize_t elementSize = sizeof(quadricsRecvFragDesc);
    ssize_t poolChunkSize = PAGESIZE;
    int nFreeLists = local_nprocs();

    bool enforceAffinity = true;
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
	  memAffinityPool, enforceAffinity,
	  ShareMemDescPool, quadricsRecvFragsDescAbortWhenNoResource
	    );

    if ( retVal ) {
	ulm_err(("Error: Can't initialize quadricsRecvFragDescs freelists!\n"));
	exit(1);
    }

    return;
}

// don't enforceAffinity since Quadrics support
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

    bool enforceAffinity = false;

    int retVal = quadricsSendFragDescs.Init
	( nFreeLists, nPagesPerList, poolChunkSize, pageSize, elementSize,
	  minPagesPerList, maxPagesPerList, maxQuadricsSendFragsDescRetries,
	  " Quadrics send frag descriptors ",
	  quadricsSendFragDescsDescRetryForResources,
	  0, enforceAffinity,
	  0, quadricsSendFragDescsDescAbortWhenNoResource);

    if ( retVal ) {
        ulm_err(("Error: Can't initialize quadricsSendFragDescs freelists!\n"));
        exit(1);
    }

    return;
}

ELAN3_CTX *getElan3Ctx(int rail) {
    return quadricsQueue[rail].ctx;
}

#ifndef offsetof
#define offsetof(T,F)   ((int)&(((T *)0)->F))
#endif

/* 
 * This is following the challenges made in src/init/init_rms.cc,
 * to accommodate the QCS update. May not cover all different quadrics 
 * releases across different platforms.
 */
#if QSNETLIBS_VERSION_CODE >= QSNETLIBS_VERSION(1,4,14)
#define elan3_nvps elan_nvps 
#define elan3_vp2location elan_vp2location
#define elan3_location2vp elan_location2vp
#define elan3_maxlocal elan_maxlocal
#define elan3_nlocal elan_nlocal
#define elan3_rails elan_rails
#define elan3_nrails elan_nrails
/*#define Node loc_node*/
/*#define Context loc_context*/
#endif

void quadricsInitQueueInfo()
{
    int pageSize = sysconf(_SC_PAGESIZE);
    E3_uint32 msize = ELAN_ALIGNUP(ELAN_ALLOC_SIZE, pageSize);
    E3_uint32 esize = ELAN_ALIGNUP(ELAN_ALLOCELAN_SIZE, pageSize);
    E3_Addr mbase = (E3_Addr)ELAN_ALLOC_BASE;
    E3_Addr ebase = (E3_Addr)ELAN_ALLOCELAN_BASE;

#ifdef  USE_ELAN_COLL
    quadricsGlobInfo_t * base;
    ELAN_CAPABILITY    * cap;
#endif

    int *dev = (int *)ulm_malloc(sizeof(int) * quadricsNRails);
    if (!dev) {
        ulm_err(("quadricsInitQueueInfo: malloc for %d ints for dev failed!\n",
                 quadricsNRails));
        exit(1);
    }

#if QSNETLIBS_VERSION_CODE >= QSNETLIBS_VERSION(1,4,14)
    if (quadricsNRails != elan_rails(&quadricsCap, dev)) {
        ulm_err(("quadricsInitQueueInfo: elan_rails does not agree with elan_nrails!\n"));
        exit(1);
    }
#else
    if (quadricsNRails != elan3_rails(&quadricsCap, dev)) {
        ulm_err(("quadricsInitQueueInfo: elan3_rails does not agree with elan3_nrails!\n"));
        exit(1);
    }
#endif

    quadricsQueue = (quadricsQueueInfo_t *)ulm_malloc(sizeof(quadricsQueueInfo_t) * quadricsNRails);
    if (!quadricsQueue) {
        ulm_err(("quadricsInitQueueInfo: quadricsQueue allocation of %d bytes failed!\n",
                 sizeof(quadricsQueueInfo_t) * quadricsNRails));
        exit(1);
    }

#ifdef USE_ELAN_COLL
    if (quadricsHW) {
	cap = &quadricsCap;

	/* Initialize the global memory structure */
	quadrics_Glob_Mem_Info = (quadricsGlobInfo_t *)
	    ulm_malloc(sizeof(quadricsGlobInfo_t));

	quadrics_Glob_Mem_Info->allCtxs = (ELAN3_CTX **)
	    ulm_malloc(sizeof(ELAN3_CTX *)*quadricsNRails);

	if (!quadrics_Glob_Mem_Info || ! quadrics_Glob_Mem_Info->allCtxs) {
	    ulm_err(("quadricsInitQueueInfo: malloc for %d bytes for quadrics_Glob_Mem_Info failed!\n",
		     sizeof(quadricsGlobInfo_t)));
	    ulm_err(("disabling quadrics hardware bcast\n"));
	    quadricsHW=0;
	}
	base = quadrics_Glob_Mem_Info;
    }
#endif

    for (int i = 0; i < quadricsNRails; i++) {

        quadricsQueueInfo_t *p = &(quadricsQueue[i]);

        new (p) quadricsQueueInfo_t;
        p->railOK = true;
        p->ctlMsgsToSendFlag = p->ctlMsgsToAckFlag = 0;


        // initalize the Elan3 device
        p->ctx = elan3_init(dev[i], mbase, msize, ebase, esize);

#ifdef USE_ELAN_COLL
        if (quadricsHW) {
            /* Stash the array of ctx. */
            quadrics_Glob_Mem_Info->allCtxs[i] = p->ctx;
            if ( i==0) base->ctx = p->ctx;
        }
#endif

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


#ifdef USE_ELAN_COLL
        /* Stash self_vp. */
        if (quadricsHW) base->self = myproc();
#endif
    }

#ifdef USE_ELAN_COLL
    /* Allocate Global Memory, code after libelan. -- Weikuan */
    if (quadricsHW) {
        /* Request the minimum about of 'Global' memory to
         * allocate for the base group
         */
        int gMainMemSize, gElanMemSize, gElanMemSizeUp;
        maddr_vm_t  gMemMain;
        sdramaddr_t gMemElan; 

        gMainMemSize = QUADRICS_GLOB_MEM_MAIN;
        gElanMemSize = QUADRICS_GLOB_MEM_ELAN;

        /* As these are mmap'ed in we need them to be PAGESIZE multiples */
        gMainMemSize = ELAN_ALIGNUP(gMainMemSize, PAGESIZE);
        gElanMemSizeUp = ELAN_ALIGNUP(gElanMemSize, PAGESIZE);

        /* Allocate 'Global' Main memory for base allGroup
 	 * This is basically what the gallocCreate() code does but we can't
	 * use that as we are boot-strapping here.
	 */
        ELAN_ADDR gMemMainAddr = ELAN_BAD_ADDR;
        int zfd;

        if ((zfd = open ("/dev/zero", O_RDWR)) < 0)
        {
            ulm_err(("Failed to open /dev/zero - check permissions!\n"
		  "Disabling optimized hardware broadcast.\n"));
            quadricsHW=0;
            goto quadricsHW_loop_exit;
        }

        for (int i = 0; i < quadricsNRails; i++)
        {
            quadricsQueueInfo_t *p = &(quadricsQueue[i]);
            ELAN_ADDR eaddr;

            if ((eaddr = elan3_allocVaddr(p->ctx, PAGESIZE, gMainMemSize)) == ELAN_ADDR_NULL) {
                ulm_err(("Failed to allocate vaddr space from Main allocator ctx %lx rail %d : %x\n",
                        p->ctx, i, (long )eaddr));
                ulm_err(("Disabling optimized hardware broadcast.\n"));
                quadricsHW=0;
                goto quadricsHW_loop_exit;
            }
            if (gMemMainAddr != ELAN_BAD_ADDR && eaddr != gMemMainAddr) {
                ulm_err(("Failed to allocate matching vaddr space from Main allocator ctx %x rail %d: %x.%x\n",
                        p->ctx, i, eaddr, gMemMainAddr));
                ulm_err(("Disabling optimized hardware broadcast.\n"));
                quadricsHW=0;
                goto quadricsHW_loop_exit;
            }else{
                /* Remember the allocated Elan address */
                gMemMainAddr = eaddr;
            }
        }

        /* Find the corresponding main memory address. */
        gMemMain = (void *) elan3_elan2main (base->ctx, gMemMainAddr);

        /* Unmap this main memory. */
        (void) munmap( gMemMain, gMainMemSize);

        /* Now map some real main memory against the 
	 * reserved Elan address space */
#ifdef USE_ELAN_SHARED
        /* use shared memory allocated pre-fork */
        gMemMain = elan_coll_sharedpool;
#else
        if ((gMemMain = mmap(gMemMain, gMainMemSize,
                             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED,
                             zfd, 0)) == (void *)-1)
        {
            ulm_err(("Failed to mmap global main memory"
		       	"Disabling optimized hardware broadcast.\n"));
            quadricsHW=0;
            goto quadricsHW_loop_exit;
        }
#endif

        /* Now map it into each rail */
        for (int i = 0; i < quadricsNRails; i++)
        {
            quadricsQueueInfo_t *p = &(quadricsQueue[i]);

            // check if memory is 32bit addressable
            if (!ELAN3_ADDRESSABLE (p->ctx, gMemMain, gMainMemSize)) {
                ulm_err(("Allocated memory is not elan3 addressable "
			    "Disabling optimized hardware broadcast.\n"));
                quadricsHW=0;
                goto quadricsHW_loop_exit;
            }

            /* Remove any previous Main mappings */
            (void)elan3_clearperm_main (p->ctx, (char *)gMemMain, gMainMemSize);

            /* Remove any previous Elan mappings */
            (void)elan3_clearperm_elan (p->ctx, gMemMainAddr, gMainMemSize);

            /* Create a new mapping from SHM to the Elan memory addresses */
            if (elan3_setperm (p->ctx, (char*)gMemMain, gMemMainAddr,
                               gMainMemSize, ELAN_PERM_REMOTEALL) < 0)
            {
                ulm_err(("Failed setperm main " 
			    "Disabling optimized hardware broadcast.\n"));
                quadricsHW=0;
                goto quadricsHW_loop_exit;
            }
            /* Stash it into the quadricsQueueInfo_t */
            quadrics_Glob_Mem_Info->allCtxs[i] = p->ctx;
        }

        /* Stash the address and length */
        quadrics_Glob_Mem_Info->globMainMem = (maddr_vm_t) gMemMain;
        quadrics_Glob_Mem_Info->globMainMem_len = QUADRICS_GLOB_MEM_MAIN;

        (void) close(zfd);

        /*
         * allocate a chunk of elan (sdram) memory from each rail from
         * the elan memory allocator (this means it comes from ctx->sdram)
         */
        {
            ELAN_ADDR gMemElanAddr = ELAN_BAD_ADDR;
            for (int i = 0; i < quadricsNRails; i++)
            {
                quadricsQueueInfo_t *p = &(quadricsQueue[i]);
                ELAN_ADDR eaddr;

                if ((gMemElan= (sdramaddr_t)elan3_allocElan
			    (p->ctx, PAGESIZE, gElanMemSize)) == NULL)
                {
                    ulm_err(("elan_baseinit: failed to allocate gmemelan space from elan allocator ctx %lx rail %d : %x\n",
                            p->ctx, i, gMemElan));
                    ulm_err(("Disabling optimized hardware broadcast.\n"));
                    quadricsHW=0;
                    goto quadricsHW_loop_exit;
                }

                eaddr = elan3_sdram2elan(p->ctx, p->ctx->sdram, (sdramaddr_t)gMemElan);

                if (gMemElanAddr != ELAN_BAD_ADDR && eaddr != gMemElanAddr)
                {
                    ulm_err(("elan_baseinit:4 failed to allocate matching vaddr from elan allocator ctx %x rail %d : %x.%x\n",
                            p->ctx, i, eaddr, gMemElanAddr));
                    ulm_err(("Disabling optimized hardware broadcast.\n"));
                    quadricsHW=0;
                    goto quadricsHW_loop_exit;
                }
                else
                    gMemElanAddr = eaddr;

                /* Stash the address */
                quadrics_Glob_Mem_Info->globElanMem = gMemElan;
            }
        }

        /* Stash the length */
        quadrics_Glob_Mem_Info->globElanMem_len = QUADRICS_GLOB_MEM_ELAN;

	/* Setup some environment parameters. */
	base->retryCount = 63;
	base->dmaType    = DMA_BYTE;
	base->waitType   = ELAN_POLL_EVENT;
	base->cap        = cap ;
	base->nrails     = quadricsNRails;
	base->nvp        = elan3_nvps(cap);
	base->myloc      = elan3_vp2location(base->self, cap);
	base->maxlocals  = elan3_maxlocal(cap);
#if QSNETLIBS_VERSION_CODE >= QSNETLIBS_VERSION(1,4,14)
	base->nlocals    = elan3_nlocal(base->myloc.loc_node, cap);
#else
	base->nlocals    = elan3_nlocal(base->myloc.Node, cap);
#endif
    }

    // if error detected above, code can disable quadricsHW and jump here:
quadricsHW_loop_exit:
#endif

    /* Break the loop into two, so that a global memory can be created */
    for (int i = 0; i < quadricsNRails; i++) {

        quadricsQueueInfo_t *p = &(quadricsQueue[i]);

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
        p->nSlots = 512; 
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
        mb();

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

        mb();

        // prime the event so it fires on the first receive
        elan3_primeevent(p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event), 1);

        /* There is a bug in the current device driver that means if the block-copy event
         * traps on the QDMA arrival things get messed up. So we workaround this by setting
         * the event first to make sure it doesn't trap subsequential
         */
        elan3_setevent(p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event));
                                                                                                                         
        /* Wait for it to fire */
        elan3_waitevent_blk(p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event), (E3_Event_Blk*)p->rcvBlk, ELAN_POLL_EVENT);
                                                                                                                         
        /* Reset and prime it again */
        E3_RESET_BCOPY_BLOCK(p->rcvBlk);
        elan3_primeevent(p->ctx, p->sdramQAddr + offsetof(E3_Queue, q_event), 1);

        mb();

        // unlock the queue
        elan3_write32_sdram(p->ctx->sdram, p->sdramQAddr + offsetof(E3_Queue, q_state), 0);
    }

#ifdef USE_ELAN_COLL
    /* Take the first rail as the representative */
    if (quadricsHW) 
       quadrics_Glob_Mem_Info->ctx = quadrics_Glob_Mem_Info->allCtxs[0];
#endif

    free(dev);
}

void quadricsInitBeforeFork() {

	/* sanity check on sizes of data elements */
	if( sizeof(quadricsDataAck) !=128 ) {
		ulm_err(("size of quadricsDataAck is : %ld - !=128 bytes \n",sizeof(quadricsDataAck) ));
               	exit(1);
	}
    // get capability
    if (rms_getcap(0, &quadricsCap) < 0) {
        ulm_err(("quadricsInitBeforeFork: rms_getcap(0, %p) failed!\n", &quadricsCap));
        exit(1);
    }

#if QSNETLIBS_VERSION_CODE >= QSNETLIBS_VERSION(1,4,14)
    // calculate quadricsNRails
    quadricsNRails = elan_nrails(&quadricsCap);
    if (quadricsNRails <= 0) {
        ulm_err(("quadricsInitBeforeFork: elan_nrails returned %d rails!\n", quadricsNRails));
        exit(1);
    }
#else
    // calculate quadricsNRails
    quadricsNRails = elan3_nrails(&quadricsCap);
    if (quadricsNRails <= 0) {
        ulm_err(("quadricsInitBeforeFork: elan(3)_nrails returned %d rails!\n", quadricsNRails));
        exit(1);
    }
#endif

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

#ifdef USE_ELAN_COLL
    /* For the purpose of utilizing quadrics hardware broadcast,
     * Set up a pool of shared memory buffer before processes fork.
     * Need to double check if the shared memory pool is available
     * when there is only one process on the current node?
     */
    if (quadricsHW) {
	FixedSharedMemPool * shmem_pool ; 
        int gMainMemSize ;

	gMainMemSize = ELAN_ALIGNUP(QUADRICS_GLOB_MEM_MAIN, PAGESIZE);
	shmem_pool   = (FixedSharedMemPool* ) &SharedMemoryPools;

        broadcasters_locks = (Locks *) shmem_pool->getMemorySegment
            (sizeof(Locks), CACHE_ALIGNMENT);
        broadcasters_locks->init();

	/* Allocate an array of flags recording the broadcast memory 
	 * in use. And Initialize them as NULL 
	 */
	busy_broadcasters = (unique_commid_t *) shmem_pool->getMemorySegment
	    (MAX_BROADCASTERS * sizeof(busy_broadcasters[0]), 
	     CACHE_ALIGNMENT);

        for (int i=0; i<MAX_BROADCASTERS; ++i) 
	    busy_broadcasters[i].cid = MPI_COMM_NULL;

        /* Allocate shared memory pool, if failed, disable quadricsHW */
        elan_coll_sharedpool = (maddr_vm_t) shmem_pool->getMemorySegment
	    (gMainMemSize, CACHE_ALIGNMENT);
        if (NULL == elan_coll_sharedpool) {
            ulm_err(("Failed to allocate shared memory for Quadrics"
		       	" hardware bcast, Reverting to software broadcast\n"));
            quadricsHW=0;
        }
    }
#endif
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
