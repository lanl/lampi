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

#include "path/quadrics/peerMemoryTracking.h"
#include "path/quadrics/header.h"
#include "path/quadrics/addrCache.h"

#ifdef USE_ELAN_COLL
#include "path/quadrics/coll.h"
#endif

#define QUADRICS_BADRAIL_DMA_TIMEOUT 60
#define QUADRICS_MEMREQ_MINRETRANSMIT 30
#define QUADRICS_MEMRLS_TIMEOUT 120
#define ELAN_BCAST_BUF_SZ 16384

struct quadricsState_t
{
    Locks	quadricsLock;		/* elan3 library is not thread safe. */
};

extern quadricsState_t		quadricsState;


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
class dmaThrottle;

extern dmaThrottle *quadricsThrottle;
extern peerMemoryTracker **quadricsPeerMemory;
extern int quadricsBufSizes[NUMBER_BUFTYPES];
extern int maxOutstandingQuadricsFrags;
extern int quadricsNRails;
extern int quadricsLastRail;
extern int quadricsDoChecksum;
extern int quadricsDoAck;
extern int quadricsHW;
extern quadricsQueueInfo_t *quadricsQueue;

#ifdef USE_ELAN_COLL
extern quadricsGlobInfo_t  *quadrics_Glob_Mem_Info ;
extern Broadcaster         ** quadrics_broadcasters;
extern int                    broadcasters_array_len ;
extern maddr_vm_t             elan_coll_sharedpool;  

/* busy_broadcasters in shared memory.  Lock/unlock with broadcasters_locks */
/* busy_broadcaster[i] = the unique communicator assigned to the shared memory used
                         by the i'th Broadcaster.
                         To identify a communicator uniquely, we need two numbers,
                         since non-overlapping communicators can have the same context id */
typedef struct unique_commid {
    int cid;               /* context id of communicator */
    int pid;               /* smalled proc id in that communicator */
} unique_commid_t;
extern Locks               *  broadcasters_locks;
extern unique_commid_t      *  busy_broadcasters;  /*[MAX_BROADCASTERS];*/
#endif

extern FreeListPrivate_t <quadricsSendFragDesc> quadricsSendFragDescs;
extern long maxQuadricsSendFragsDescRetries;
extern bool quadricsSendFragDescsDescRetryForResources;
extern bool quadricsSendFragDescsDescAbortWhenNoResource;
extern FreeListShared_t <quadricsRecvFragDesc> quadricsRecvFragDescs;
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
