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


#ifndef QUADRICS_STATE_H
#define QUADRICS_STATE_H

#include <elan3/elan3.h>

#include "util/Lock.h"
#include "internal/mmap_params.h"
#include "mem/FreeLists.h"
#include "queue/SeqTrackingList.h"

#include "path/quadrics/dmaThrottle_new.h"
#include "path/quadrics/peerMemoryTracking.h"
#include "path/quadrics/header.h"
#include "path/quadrics/addrCache.h"

#define QUADRICS_BADRAIL_DMA_TIMEOUT 60
#define QUADRICS_MEMREQ_MINRETRANSMIT 30
#define QUADRICS_MEMRLS_TIMEOUT 120
#define ELAN_BCAST_BUF_SZ 16384

typedef struct quadricsQueueInfo {
    bool railOK;
    Locks rcvLock;			 /* thread lock */
    Locks ctlFlagsLock;		 /* thread lock */
    E3_Addr elanQAddr;
    sdramaddr_t sdramQAddr;
    sdramaddr_t doneBlk;
    volatile E3_Event_Blk *rcvBlk;
    quadricsCtlHdr_t *queueSlots;
    E3_Addr elanQueueSlots;
    ELAN3_CTX *ctx;
    caddr_t q_base;
    caddr_t q_top;
    caddr_t q_fptr;
    long nSlots;
    unsigned int ctlMsgsToSendFlag;
    unsigned int ctlMsgsToAckFlag;
    ProcessPrivateMemDblLinkList ctlMsgsToSend[NUMBER_CTLMSGTYPES];
    ProcessPrivateMemDblLinkList ctlMsgsToAck[NUMBER_CTLMSGTYPES];
} quadricsQueueInfo_t;

class quadricsSendFragDesc;
class quadricsRecvFragDesc;

extern dmaThrottle *quadricsThrottle;
extern peerMemoryTracker **quadricsPeerMemory;
extern int quadricsBufSizes[NUMBER_BUFTYPES];
extern int maxOutstandingQuadricsFrags;
extern int quadricsNRails;
extern int quadricsLastRail;
extern int quadricsDoChecksum;
extern int quadricsDoAck;
extern quadricsQueueInfo_t *quadricsQueue;
extern FreeLists<DoubleLinkList, quadricsSendFragDesc, int, MMAP_PRIVATE_PROT,
                 MMAP_PRIVATE_FLAGS, MMAP_SHARED_FLAGS> quadricsSendFragDescs;
extern long maxQuadricsSendFragsDescRetries;
extern bool quadricsSendFragDescsDescRetryForResources;
extern bool quadricsSendFragDescsDescAbortWhenNoResource;
extern FreeLists<DoubleLinkList, quadricsRecvFragDesc, int, MMAP_SHARED_PROT,
                 MMAP_SHARED_FLAGS, MMAP_SHARED_FLAGS> quadricsRecvFragDescs;
extern long maxQuadricsRecvFragsDescRetries;
extern bool quadricsRecvFragsDescRetryForResources;
extern bool quadricsRecvFragsDescAbortWhenNoResource;
extern ELAN_CAPABILITY quadricsCap;
extern SeqTrackingList *quadricsMemRlsSeqList;
extern long long *quadricsMemRlsSeqs;
extern Locks sndMemRlsSeqsLock;
extern double quadricsLastMemRls;
extern int quadricsLastMemRlsRail;
extern int quadricsLastMemRlsDest;
extern addrCache quadricsHdrs;

/* prototypes */
void quadricsInitBeforeFork();
void quadricsInitAfterFork();

#endif
