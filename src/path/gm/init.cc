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

#include <gm.h>

#include "init/init.h"
#include "path/gm/state.h"
#include "path/gm/recvFrag.h"
#include "path/gm/sendFrag.h"

// Initialize GM with a receive buffer for every receive token 

static void initRecvBuffers(void)
{
    int i, rc;

    for (i = 0; i < gmState.nDevsAllocated; i++) {
        localDevInfo_t *devInfo = &(gmState.localDevList[i]);

        // feed GM all the buffers it can take since we
        // won't be back for an indeterminate amount of time
        if (devInfo->recvTokens) {
            if (usethreads())
                devInfo->Lock.lock();
            
            while (devInfo->recvTokens) {
                // get another buffer
                gmFragBuffer *buf = devInfo->bufList.getElementNoLock(0, rc);
                if (rc != ULM_SUCCESS) {
                    break;
                }
            
                // give it to GM...
                buf->me = buf;
                void *addr = &(buf->header);
                gm_provide_receive_buffer_with_tag(devInfo->gmPort,
                    addr, gmState.log2Size, GM_LOW_PRIORITY, i);
            
                // decrement recvTokens
                (devInfo->recvTokens)--;

            }
            
            if (usethreads())
                devInfo->Lock.unlock();
        }
    }
}

// Initialize send and recv fragment freelists

static void initFragFreelists(void)
{
    int nFreeLists;
    long nPagesPerList;
    ssize_t PoolChunkSize;
    size_t PageSize;
    ssize_t ElementSize;
    long minPagesPerList;
    long maxPagesPerList;
    long mxConsecReqFailures;
    int retryForMoreResources;
    int *affinity;
    bool enforceMemAffinity;
    bool Abort;
    int threshToGrowList;
    int i;
    int rc;
    int totalRecvTokens = 0, totalSendTokens = 0;

    // allocate and initialize memory affinity index

    affinity = (int *) ulm_malloc(sizeof(int) * local_nprocs());
    if (!affinity) {
        ulm_exit((-1, "Error: gmRecvDescs: Out of memory\n"));
    }
    for (i = 0; i < local_nprocs(); i++) {
        affinity[i] = i;
    }
    
    for (i = 0; i < gmState.nDevsAllocated; i++) {
        totalRecvTokens += gmState.localDevList[i].recvTokens;
        totalSendTokens += gmState.localDevList[i].sendTokens;
    }

    // recv fragment freelist: this is in private memory for now; in the past
    // this would be in shared memory so that we can
    // support a multicast path -- in that case this must
    // be initialized PREFORK!

    rc = gmState.recvFragList.Init(nFreeLists = 1,
                                   nPagesPerList = ((totalRecvTokens * sizeof(gmRecvFragDesc)) / getpagesize()) + 1,
                                   PoolChunkSize = getpagesize(),
                                   PageSize = getpagesize(),
                                   ElementSize = sizeof(gmRecvFragDesc),
                                   minPagesPerList = ((totalRecvTokens * sizeof(gmRecvFragDesc)) / getpagesize()) + 1,
                                   maxPagesPerList = -1,
                                   mxConsecReqFailures = 1000,
                                   "GM recv frag descriptors",
                                   retryForMoreResources = true,
                                   NULL,
                                   enforceMemAffinity = false,
                                   NULL,
                                   Abort = true,
                                   threshToGrowList = 0);
    if (rc) {
        ulm_exit((-1, "Error: Can't initialize recvFragList\n"));
    }

    // send fragment freelist: in private memory

    rc = gmState.sendFragList.Init(nFreeLists = 1,
                                   nPagesPerList = ((totalSendTokens * sizeof(gmSendFragDesc)) / getpagesize()) + 1,
                                   PoolChunkSize = getpagesize(),
                                   PageSize = getpagesize(),
                                   ElementSize = sizeof(gmSendFragDesc),
                                   minPagesPerList = ((totalSendTokens * sizeof(gmSendFragDesc)) / getpagesize()) + 1,
                                   maxPagesPerList = -1,
                                   mxConsecReqFailures = 1000,
                                   "GM send frag descriptors",
                                   retryForMoreResources = true,
                                   NULL,
                                   enforceMemAffinity = false,
                                   NULL,
                                   Abort = true,
                                   threshToGrowList = 0);
    if (rc) {
        ulm_exit((-1, "Error: Can't initialize sendFragList\n"));
    }
}


void gmSetup(lampiState_t *s)
{
    char portName[15];
    char tmpMacAddress[LENMACADDR];
    gm_port *tmpPort;
    gm_status_t returnValue;
    int *nDevsPerProc;
    int dev;
    int i,*devs,proc,rDevIndex;
    int j;
    int localDev;
    int maxDevs;
    int port;
    int rc;
    int reserved;
    localBaseDevInfo_t *allBaseDevInfo, *localBaseDevInfo;
    unsigned int tmpNodeID;

    /* sanity checks */
    if( sizeof(gmHeaderDataAck) != HEADER_SIZE ) {
	    ulm_err((" sizeof gmHeaderDataAck = %ld - should be %ld \n",sizeof(gmHeaderDataAck),HEADER_SIZE));
	    s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
    	    return;
    }

    unsigned long long PoolSize;
    long long maxLen;
    size_t PgSize;
    int Nlsts, retryForMoreResources, threshToGrowList;
    long nPagesPerList, minPagesPerList, maxPagesPerList, mxConsecReqFailures;
    ssize_t PoolChunkSize, ElementSize;
    size_t PageSize;
    char *lstlistDescription;
    int *affinity;
    bool enforceMemAffinity, Abort, freeMemPool;
    MemoryPoolPrivate_t *inputPool;

    if (usethreads())
        gmState.gmLock.lock();

    if (gmState.inited) {
        if (usethreads())
            gmState.gmLock.unlock();
        return;
    }

    if (!s->iAmDaemon) {
        // Initialize gm

        returnValue = gm_init();
        if (returnValue != GM_SUCCESS) {
            ulm_err(("Error: Can't initialize GM (%d)\n", (int) returnValue));
            s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
            return;
        }
        // open local devices

        gmState.nDevsAllocated = 0;
        /* loop over devices */
        for (dev = 0; dev < gmState_t::MAX_GM_DEVICES; dev++) {
            /* loop over possible ports */
            for (port = 0; port < gmState_t::MAX_PORTS_PER_DEVICE; port++) {
                /* check and see if this port is a reserved port */
                reserved = 0;
                for (i = 0; i < gmState_t::NUM_RESERVED_PORTS; i++) {
                    if (gmState.reservedPorts[i] == port) {
                        reserved = 1;
                        break;
                    }
                }                   /* end i loop */
                /* don't try and use reserved port */
                if (reserved)
                    continue;

                /* try and open port */
                sprintf(portName, "%s", "lampi");
                sprintf(portName + 5, "%d:", dev);
                if (dev < 10)
                    sprintf(portName + 7, "%d", port);
                else if (dev < 100)
                    sprintf(portName + 8, "%d", port);
                else {
                    ulm_err(("Error: Device index too large (%d)\n", dev));
                    s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
                    return;
                }

                returnValue =
                    gm_open(&tmpPort, dev, port, portName,
                            (enum gm_api_version)GM_API_VERSION);
                if (returnValue == GM_NO_SUCH_DEVICE) {
                    break;
                }
                else if (returnValue != GM_SUCCESS) {
                    continue;
                }

                /* 
                 * got port 
                 */
                gm_get_node_id(tmpPort, &tmpNodeID);
                gmState.localDevList[gmState.nDevsAllocated].node_id = tmpNodeID;

/* GM 2.x now uses local node ID and a global node ID.  For versions earlier
	than 2.x, the local ID is the same as the global ID.
*/
#if GM_API_VERSION >= 0x200				
                returnValue = gm_node_id_to_global_id(tmpPort, tmpNodeID, 
                                &(gmState.localDevList[gmState.nDevsAllocated].global_node_id));
                if (returnValue != GM_SUCCESS) {
                    ulm_err(("Error: gm_node_id_to_global_id() node %d port %d"
                       " returned %d\n", dev, port, (int) returnValue));
                    s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
                    return;
                }
#else
                gmState.localDevList[gmState.nDevsAllocated].global_node_id = tmpNodeID;             
#endif

                gmState.localDevList[gmState.nDevsAllocated].port_id = port;
                gmState.localDevList[gmState.nDevsAllocated].gmPort =
                    tmpPort;
                gmState.localDevList[gmState.nDevsAllocated].sendTokens =
                    gm_num_send_tokens(tmpPort);
                gmState.localDevList[gmState.nDevsAllocated].recvTokens =
                    gm_num_receive_tokens(tmpPort);
                returnValue =
                    gm_get_unique_board_id(tmpPort,
                                       gmState.localDevList[gmState.
                                                            nDevsAllocated].
                                       macAddress);
                if (returnValue != GM_SUCCESS) {
                    ulm_err(("Error: gm_get_unique_board_id() node %d port %d"
                       " returned %d\n", dev, port, (int) returnValue));
                    s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
                    return;
                }

                /* record device as opened */
                gmState.nDevsAllocated++;

                /* open only one port per device */
                break;

            }                       /* end port loop */

            /* see if limit on number of ports reached.
            *   if no limit set of maxGMDevs, this value
            *   will be negative, and the condition never met */
            if ( (gmState.nDevsAllocated == gmState.maxGMDevs) ||
                 (GM_NO_SUCH_DEVICE == returnValue) )
                break;

        }                           /* end dev loop */
    }

    if ( 0 == gmState.nDevsAllocated )
        ulm_warn(("Process %d: Warning! No Myrinet GM devices found!\n",myproc()));

    /*
     * gather hostID, nodeID, portID, and MAC address for
     *   all remote NIC's used in the job
     */
    /* gather the number of devices for each process */
    nDevsPerProc = (int *) ulm_malloc(sizeof(int) * s->global_size);
    if (!nDevsPerProc) {
        ulm_err(("Error: Out of memory\n"));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    rc = s->client->allgather(&gmState.nDevsAllocated, nDevsPerProc,
                              sizeof(int));
    if (rc != ULM_SUCCESS) {
        ulm_err(("Error: Returned from allgather (%d)\n", rc));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    /* 
     * gather all the data 
     */
    /* for now, since we don't have allgatherv implemented, use allgather
     *   to get all the data, padding with 0's where needed
     */
    /* figure out process with max number of GM devices */
    maxDevs = 0;
    for (i = 0; i < s->global_size; i++) {
        if (nDevsPerProc[i] > maxDevs)
            maxDevs = nDevsPerProc[i];
    }

#ifndef ENABLE_CT
    if ((myhost() == 0) && ((s->useDaemon && s->iAmDaemon) || 
        (!s->useDaemon && (local_myproc() == 0)))) {
        /* send maxDevs to mpirun */
        s->client->reset(adminMessage::SEND);
        s->client->pack(&maxDevs, adminMessage::INTEGER, 1);
        s->client->send(-1, adminMessage::GMMAXDEVS, &rc);
    }
#endif

    if (maxDevs == 0) {
        ulm_err(("Error: No GM devices configured\n"));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    /* reset maxGMDevs to the actual number */
    gmState.maxGMDevs = maxDevs;

    /* allocate space to hold compact information about the local devices */
    localBaseDevInfo = (localBaseDevInfo_t *) ulm_malloc(sizeof(localBaseDevInfo_t) * maxDevs);
    if (!localBaseDevInfo) {
        ulm_err(("Error: Out of memory\n"));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    if (!s->iAmDaemon) {
        for (i = 0; i < maxDevs; i++) {
            if (i < gmState.nDevsAllocated) {
                localBaseDevInfo[i].node_id = gmState.localDevList[i].node_id;
                localBaseDevInfo[i].global_node_id = gmState.localDevList[i].global_node_id;
                localBaseDevInfo[i].port_id = gmState.localDevList[i].port_id;
                bcopy(gmState.localDevList[i].macAddress, localBaseDevInfo[i].macAddress,
                    LENMACADDR);
            }
            else {
                localBaseDevInfo[i].node_id = (unsigned int)-1;
                localBaseDevInfo[i].global_node_id = (unsigned int)-1;
                localBaseDevInfo[i].port_id = (unsigned int)-1;
            }
        }
    }

    /* allocate space to store device information from all hosts */
    allBaseDevInfo =
        (localBaseDevInfo_t *) ulm_malloc(sizeof(localBaseDevInfo_t) *
                                      s->global_size * maxDevs);
    if (!allBaseDevInfo) {
        ulm_err(("Error: Out of memory\n"));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    /* accumulate remote device information */
    rc = s->client->allgather(localBaseDevInfo, allBaseDevInfo,
                              sizeof(localBaseDevInfo_t) * maxDevs);
    if (rc != ULM_SUCCESS) {
        ulm_err(("Error: Returned from allgather (%d)\n", rc));
        s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
        return;
    }

    if (!s->iAmDaemon) {
	    /* allocate memory for remote device list for each local device */
	    for (i = 0; i < gmState.nDevsAllocated; i++) {
	        gmState.localDevList[i].remoteDevList =
	            (remoteDevInfo_t *) ulm_malloc(sizeof(remoteDevInfo_t) *
	                                           s->global_size);
	        if (!gmState.localDevList[i].remoteDevList) {
	            ulm_err(("Error: allocating memory for %d remoteDevList"
                   " (processes %d" " local device count %d)\n", i, 
                   s->global_size, gmState.nDevsAllocated));
	            s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
	            return;
	        }
	        /* initialize node and port ids to "unused" value, -1 */
	        for (j = 0; j < s->global_size; j++) {
	            gmState.localDevList[i].remoteDevList[j].node_id =
	                (unsigned int) -1;
	            gmState.localDevList[i].remoteDevList[j].global_node_id =
	                (unsigned int) -1;
	            gmState.localDevList[i].remoteDevList[j].port_id =
	                (unsigned int) -1;
	        }
	    }
	
	    /*
	     * figure out network connectivity
	     */
            /* loop over all procs */
            devs=(int *)ulm_malloc(sizeof(int)*maxDevs);
            for(proc=0 ; proc < s->global_size ; proc++ ) {
	          /* loop over local devices */
	          for (localDev = 0; localDev < gmState.nDevsAllocated; 
                    localDev++) {
                      /* figure out how many remote devices one can
                       *   commnicate with via this local device */
                       int count=0;
                       for(i=0 ; i < maxDevs ; i++)
                         devs[i]=-1;
	               for( rDevIndex = proc * maxDevs;
                            rDevIndex < (proc+1) * maxDevs ;
                            rDevIndex++ ) {
	                  /* check to make sure this is not just dummied up data */
	                  if ( (allBaseDevInfo[rDevIndex].node_id == 
                                (unsigned int)-1))
	                  continue;
                          /* check and see if the devices are connected */
#if GM_API_VERSION >= 0x200
	                  gm_global_id_to_node_id(
                              gmState.localDevList[localDev].gmPort,
                              allBaseDevInfo[rDevIndex].global_node_id,
                              &(allBaseDevInfo[rDevIndex].node_id));
#endif
	                  /* check to see if remote device is reachable from
	                   *   the local device */
	                  returnValue =
	                      gm_node_id_to_unique_id(gmState.
                               localDevList[localDev].gmPort,
	                       allBaseDevInfo[rDevIndex].node_id,tmpMacAddress);
	                  if (returnValue != GM_SUCCESS) {
	                      continue;
	                  }
	                  /* compare mac addresses to see if rDevIndex matches
	                   *   up with the device accessible by the local
	                   *   device */
	                  if ((rc = memcmp
	                      (tmpMacAddress, allBaseDevInfo[rDevIndex].macAddress,
	                       LENMACADDR)) != 0) {
	                      /* if both mac addresses match, the return
	                       * value from memcmp is 0 */
	                      continue;
	                  }
                          devs[count]=rDevIndex;
                          count++;

                       }  /*  end rDevIndex loop */
                       /* now we know how many remote devices we can connect
                        *   to, and which ones they are.  Set connectivity
                        *   information */
                        rDevIndex= localDev%count;
                        rDevIndex=devs[rDevIndex];
	                gmState.localDevList[localDev].remoteDevList[proc].
                          node_id=allBaseDevInfo[rDevIndex].node_id;
	                gmState.localDevList[localDev].remoteDevList[proc].
                          port_id=allBaseDevInfo[rDevIndex].port_id;
                  }
            }  /* end proc loop */
            ulm_free(devs);
	
	    /* initialize pinned memory buffer pool and freelist for each local device */
	    for (i = 0; i < gmState.nDevsAllocated; i++) {
	        unsigned long long initialSize = 300 * sizeof(gmFragBuffer);
	        long long poolChunkSize = 50 * sizeof(gmFragBuffer);
	        gmState.localDevList[i].memPool.gmPort =
	            gmState.localDevList[i].gmPort;
	        rc = gmState.localDevList[i].memPool.Init(PoolSize = initialSize, 
                                                      maxLen = -1,
	                                                  (long long)PoolChunkSize = poolChunkSize,
	                                                  PgSize = getpagesize());
	        if (rc != ULM_SUCCESS) {
	            ulm_err(("Error: GM memory pool initialization "
	                     "(local device %d) failed with error %d\n", i, rc));
	            s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
	            return;
	        }
	        rc = gmState.localDevList[i].bufList.Init(Nlsts = 1,
	                                                  nPagesPerList = 0,
	                                                  PoolChunkSize = poolChunkSize,
	                                                  PageSize = getpagesize(),
	                                                  ElementSize = sizeof(gmFragBuffer),
	                                                  minPagesPerList = (poolChunkSize / getpagesize()),
	                                                  maxPagesPerList = -1,
	                                                  mxConsecReqFailures = 1000,
	                                                  lstlistDescription = "GM buffer list",
	                                                  retryForMoreResources = 1,
	                                                  affinity = 0,
	                                                  enforceMemAffinity = false,
	                                                  inputPool = &(gmState.localDevList[i].memPool),
                                                      Abort = true,
                                                      threshToGrowList = 0,
                                                      freeMemPool = false);
	        if (rc != ULM_SUCCESS) {
	            ulm_err(("Error: GM buffer list initialization "
	                     "(local device %d) failed with error %d\n", i, rc));
	            s->error = ERROR_LAMPI_INIT_POSTFORK_GM;
	            return;
	        }
        }

        /* initialize send and recv fragment lists - uses send/recv token max. count */

        initFragFreelists();

        /* give GM a receive buffer for every receive token */

        initRecvBuffers();

    } // if (!s->iAmDaemon)

    /* free local resources */
    ulm_free(nDevsPerProc);
    ulm_free(allBaseDevInfo);
    ulm_free(localBaseDevInfo);

    gmState.inited = true;

    if (usethreads())
       gmState.gmLock.unlock();
}
