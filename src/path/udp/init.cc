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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/SocketGeneric.h"
#include "client/daemon.h"
#include "queue/globals.h"   // for ShareMemDescPool
#include "internal/log.h"
#include "internal/malloc.h"
#include "path/udp/path.h"

//
// Initialize the UDP receive fragment descriptors
//
static void initRecvFragmentDescriptors(void)
{
    int rc;
    long minPagesPerList = 4;
    long maxPagesPerList = -1;
    long nPagesPerList = 4;

    ssize_t pageSize = SMPPAGESIZE;
    ssize_t elementSize = sizeof(udpRecvFragDesc);
    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = local_nprocs();

    long maxUDPRecvFragsDescRetries          = 1000;
    bool UDPRecvFragsDescRetryForResources   = false;
    bool UDPRecvFragsDescAbortWhenNoResource = true;

    bool enforceAffinity = true;
    int *memAffinityPool = (int *)ulm_malloc(sizeof(int) * nFreeLists);
    if (!memAffinityPool) {
	ulm_exit(("Unable to allocate space for memAffinityPool\n"));
    }

    // fill in memory affinity index
    for(int i = 0 ; i < nFreeLists ; i++ ) {
	memAffinityPool[i] = i;
    }

    rc = UDPRecvFragDescs.Init(nFreeLists,
                               nPagesPerList,
                               poolChunkSize,
                               pageSize,
                               elementSize,
                               minPagesPerList,
                               maxPagesPerList,
                               maxUDPRecvFragsDescRetries,
                               " UDP recv frag descriptors ",
                               UDPRecvFragsDescRetryForResources,
                               memAffinityPool,
                               enforceAffinity,
                               0,
                               UDPRecvFragsDescAbortWhenNoResource);
    // clean up
    ulm_free(memAffinityPool);

    if (rc) {
	ulm_exit(("FreeLists::Init Unable to initialize"
                  " UDP recv frag descriptor pool\n"));
    }
}


//
// Initialize the send fragment descriptors
//
static void initSendFragmentDescriptors(void)
{
    int rc;

    long minPagesPerList = 4;
    long maxPagesPerList = -1;
    long nPagesPerList = 4;

    ssize_t pageSize = SMPPAGESIZE;
    ssize_t elementSize = sizeof(udpSendFragDesc);
    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = local_nprocs();

    long maxUDPSendFragsDescRetries          = 1000;
    bool UDPSendFragsDescRetryForResources   = false;
    bool UDPSendFragsDescAbortWhenNoResource = true;

    bool enforceAffinity = true;
    int *memAffinityPool = (int *)ulm_malloc(sizeof(int) * nFreeLists);
    if (!memAffinityPool) {
	ulm_exit(("Unable to allocate space for memAffinityPool\n"));
    }

    // fill in memory affinity index
    for(int i = 0 ; i < nFreeLists ; i++ ) {
	memAffinityPool[i] = i;
    }

    /* !!!! IPC lock */
    rc = udpSendFragDescs.Init(nFreeLists,
                               nPagesPerList,
                               poolChunkSize,
                               pageSize,
                               elementSize,
                               minPagesPerList,
                               maxPagesPerList,
                               maxUDPSendFragsDescRetries,
                               " UDP send frag descriptors ",
                               UDPSendFragsDescRetryForResources,
                               memAffinityPool,
                               enforceAffinity,
                               NULL,
                               UDPSendFragsDescAbortWhenNoResource);

    // clean up 
    ulm_free(memAffinityPool);

    if (rc) {
	ulm_exit(("FreeLists::Init Unable to initialize"
                  " UDP send frag descriptor pool\n"));
    }
}


//
// Initialize the send fragment buffers
//
static void initEarlySendFragmentBuffers(void)
{
    bool enforce_mem_policy;
    char *desc_string;
    int rc;
    int num_free_list;
    long minPagesPerList;
    long maxPagesPerList;
    long nPagesPerList;
    ssize_t pageSize;
    ssize_t elementSize;
    ssize_t poolChunkSize;

    long maxUDPEarlySendDescRetries          = 10;
    bool UDPEarlySendDescRetryForResources   = false;
    bool UDPEarlySendDescAbortWhenNoResource = true;

    desc_string = "UDP large early send";
    minPagesPerList = 4;
    maxPagesPerList = -1;
    nPagesPerList = 4;

    num_free_list = local_nprocs();
    enforce_mem_policy = false;

    pageSize = SMPPAGESIZE;

    //
    // Create free list for large size early send buffers
    //
    poolChunkSize = (size_t) 6 *SMPPAGESIZE;
    elementSize = sizeof(UDPEarlySend_large);
    rc = UDPEarlySendData_large.Init(num_free_list, nPagesPerList,
                                     poolChunkSize, pageSize, elementSize,
                                     minPagesPerList, maxPagesPerList,
                                     maxUDPEarlySendDescRetries,
                                     desc_string,
                                     UDPEarlySendDescRetryForResources,
                                     NULL, enforce_mem_policy, NULL,
                                     UDPEarlySendDescAbortWhenNoResource);

    if (rc) {
        ulm_exit(("FreeLists::Init Unable to initialize"
                  " UDP Early Send (large) pool\n"));
    }
    //
    // Create free list for med size early send buffers
    //
    poolChunkSize = (ssize_t) 4 *SMPPAGESIZE;
    nPagesPerList = 4;
    desc_string = "UDP med early send";
    elementSize = sizeof(UDPEarlySend_med);
    rc = UDPEarlySendData_med.Init(num_free_list, nPagesPerList,
                                   poolChunkSize, pageSize, elementSize,
                                   minPagesPerList, maxPagesPerList,
                                   maxUDPEarlySendDescRetries, desc_string,
                                   UDPEarlySendDescRetryForResources, NULL,
                                   enforce_mem_policy, NULL,
                                   UDPEarlySendDescAbortWhenNoResource);

    if (rc) {
        ulm_exit(("FreeLists::Init Unable to initialize"
                  " UDP Early Send (med) pool\n"));
    }
    // 
    // Create free list for small early send buffers
    //
    poolChunkSize = (size_t) SMPPAGESIZE;
    elementSize = sizeof(UDPEarlySend_small);
    desc_string = "UDP small early send";
    nPagesPerList = 10;
    rc = UDPEarlySendData_small.Init(num_free_list, nPagesPerList,
                                     poolChunkSize, pageSize,
                                     elementSize, minPagesPerList,
                                     maxPagesPerList,
                                     maxUDPEarlySendDescRetries,
                                     " UDP small early send ",
                                     UDPEarlySendDescRetryForResources,
                                     NULL, enforce_mem_policy, NULL,
                                     UDPEarlySendDescAbortWhenNoResource);

    if (rc) {
        ulm_exit(("FreeLists::Init Unable to initialize"
                  " UDP Early Send (small) pool\n"));
    }
}


//-----------------------------------------------------------------------------
// Initialize UDP descriptor and frag lists.
//-----------------------------------------------------------------------------
int initLocalUDPSetup(void)
{
    initRecvFragmentDescriptors();
    initSendFragmentDescriptors();
    initEarlySendFragmentBuffers();

    return ULM_SUCCESS;
}

