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

#include <elan3/elan3.h>

#include "path/quadrics/recvFrag.h"
#include "path/quadrics/sendFrag.h"

/* instantiate Quadrics-related globals */

dmaThrottle *quadricsThrottle;
peerMemoryTracker **quadricsPeerMemory;

/* WARNING: (large / max # rails) > small or sendMemoryRequest() will not
 * work correctly
 */
int quadricsBufSizes[NUMBER_BUFTYPES] = {
    2048,
    65536
};

quadricsState_t	quadricsState;

int maxOutstandingQuadricsFrags = 20;
int quadricsNRails = 0;
int quadricsLastRail = -1;
int quadricsDoChecksum = 1;
int quadricsDoAck = 1;
quadricsQueueInfo_t *quadricsQueue = 0;

FreeListPrivate_t <quadricsSendFragDesc> quadricsSendFragDescs;
long maxQuadricsSendFragsDescRetries		 = 1000;
bool quadricsSendFragDescsDescRetryForResources   = false;
bool quadricsSendFragDescsDescAbortWhenNoResource = true;

#ifdef USE_ELAN_COLL
/* The global memory available for elan3 */
quadricsGlobInfo_t  *  quadrics_Glob_Mem_Info = 0;
Locks                  broadcasters_locks;
Broadcaster         ** quadrics_broadcasters;
int                    broadcasters_array_len = 0;
char                   busy_broadcasters[MAX_BROADCASTERS] = { 0 } ;
#endif

FreeListShared_t <quadricsRecvFragDesc> quadricsRecvFragDescs;
long maxQuadricsRecvFragsDescRetries		 = 1000;
bool quadricsRecvFragsDescRetryForResources   = false;
bool quadricsRecvFragsDescAbortWhenNoResource = true;

ELAN_CAPABILITY quadricsCap;

SeqTrackingList *quadricsMemRlsSeqList = 0;
long long *quadricsMemRlsSeqs = 0;
Locks sndMemRlsSeqsLock;
double quadricsLastMemRls;
int quadricsLastMemRlsRail = -1;
int quadricsLastMemRlsDest = -1;
addrCache quadricsHdrs(32);
