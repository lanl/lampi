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

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <new>

#include "queue/Group.h"
#include "queue/globals.h"
#include "client/ULMClient.h"
#include "util/Lock.h"
#include "collective/coll_tree.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/system.h"
#include "internal/types.h"
#include "ulm/ulm.h"

#ifdef _STANDALONE              // allows the code to be debugged
#define ABORT()         return(-1)
#define MYBOX()         0
#define NBOXES()        1
#define global_proc_to_host(X)  X
#else
#define ABORT()         ulm_exit((-1, "Aborting"));
#define MYBOX()         myhost()
#define NBOXES()        lampiState.nhosts
#define global_proc_to_host(X)  global_proc_to_host(X)
#endif

Group::Group()
{
    groupID = -1;
    refCount = 0;
    mapGroupProcIDToGlobalProcID = NULL;
    mapGlobalProcIDToGroupProcID = NULL;
    numberOfHostsInGroup = 0xdeadbeef;
    groupLock.init();           // initialize lock
}


Group::~Group()
{
}

/*
 * Dump out the group structure (for debugging)
 */
void Group::dump()
{
    enum { DUMP = 0 };

    if (DUMP == 0) {
        return;
    }

    fprintf(stderr, "Group::dump()...\n");

#define DUMP_INT(X) fprintf(stderr, "    %s = %d\n", # X, X);
#define DUMP_ARRAY(X,SIZE) \
    fprintf(stderr, "    %s[] = ", # X); \
    for (int n = 0; n < SIZE; n++) { \
        fprintf(stderr, " %d", X[n]); \
    } \
    fprintf(stderr, "\n");

    DUMP_INT(ProcID);
    DUMP_INT(groupSize);
    DUMP_INT(hostIndexInGroup);
    DUMP_INT(numberOfHostsInGroup);
    DUMP_INT(onHostProcID);
    DUMP_INT(onHostGroupSize);
    DUMP_INT(maxOnHostGroupSize);
    DUMP_INT(refCount);
    DUMP_INT(groupID);

    DUMP_ARRAY(mapGroupProcIDToHostID, groupSize);
    DUMP_ARRAY(mapGlobalProcIDToGroupProcID, nprocs());
    DUMP_ARRAY(mapGroupProcIDToGlobalProcID, groupSize);
    DUMP_ARRAY(mapGroupProcIDToOnHostProcID, groupSize);
    DUMP_ARRAY(groupHostDataLookup, nhosts());

    for (int i = 0; i < numberOfHostsInGroup; i++) {
        fprintf(stderr, "groupHostData[%d]...\n", i);
        DUMP_ARRAY(groupHostData[i].groupProcIDOnHost, groupHostData[i].nGroupProcIDOnHost);
        DUMP_INT(groupHostData[i].nGroupProcIDOnHost);
        DUMP_INT(groupHostData[i].hostID);
        DUMP_INT(groupHostData[i].hostCommRoot);
        DUMP_INT(groupHostData[i].destProc);
    }

#undef DUMP_INT
#undef DUMP_ARRAY

    fflush(stderr);
}


int Group::create(int grpSize, int *rnkLst, int grpID)
{
    int *groupHostList;

    // set group index
    groupID = grpID;

    // set group size
    groupSize = grpSize;

    // set reference count
    refCount = 1;

    // set number of hosts in group
    groupHostList = (int *) ulm_malloc(sizeof(int) * lampiState.nhosts);
    if (!groupHostList) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    numberOfHostsInGroup = 0;
    for (int proc = 0; proc < groupSize; proc++) {
        int hostID = global_proc_to_host(rnkLst[proc]);
        // check to see if host already in list
        bool hostFound = false;
        for (int host = 0; host < numberOfHostsInGroup; host++) {
            if (groupHostList[host] == hostID) {
                hostFound = true;
                break;
            }
        }
        if (!hostFound) {
            groupHostList[numberOfHostsInGroup] = hostID;
            numberOfHostsInGroup++;
        }
    }

    // set hostIndexInGroup
    hostIndexInGroup = -1;
    for (int host = 0; host < numberOfHostsInGroup; host++) {
        if (groupHostList[host] == myhost()) {
            hostIndexInGroup = host;
            break;
        }
    }

    // set rankList
    mapGroupProcIDToGlobalProcID = ulm_new(int, groupSize);
    if (!mapGroupProcIDToGlobalProcID) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // if rnkLst is null - 1<->1 correspondence of ProcID and comm world
    if (rnkLst == NULL) {
        for (int i = 0; i < groupSize; i++)
            mapGroupProcIDToGlobalProcID[i] = i;
    } else {
        for (int i = 0; i < groupSize; i++) {
            mapGroupProcIDToGlobalProcID[i] = rnkLst[i];
        }
    }

    // check for duplicate entries in mapGroupProcIDToGlobalProcID list
    for (int proc1 = 1; proc1 < groupSize; proc1++) {
        for (int proc2 = 0; proc2 < proc1; proc2++) {
            if (mapGroupProcIDToGlobalProcID[proc1] ==
                mapGroupProcIDToGlobalProcID[proc2]) {
                ulm_err(("Error: Process duplicated in group list (%d)\n",
                         proc1));
                return ULM_ERR_BAD_PARAM;
            }
        }
    }

    // fill in groupHostData array
    groupHostData = ulm_new(ulm_host_info_t, numberOfHostsInGroup);
    if (!groupHostData) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    for (int host = 0; host < numberOfHostsInGroup; host++) {
        // set host id
        groupHostData[host].hostID = groupHostList[host];
        // setup list of processes on this host
        int nGroupProcs = 0;
        int *tmpProcList = (int *) ulm_malloc(sizeof(int) * nprocs());
        if (!tmpProcList) {
            ulm_err(("Error: Out of memory\n"));
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        for (int proc = 0; proc < groupSize; proc++) {
            if (global_proc_to_host(mapGroupProcIDToGlobalProcID[proc]) ==
                groupHostList[host]) {
                tmpProcList[nGroupProcs] = proc;
                nGroupProcs++;
            }
        }

        // set number of processes on host
        groupHostData[host].nGroupProcIDOnHost = nGroupProcs;
        groupHostData[host].groupProcIDOnHost = ulm_new(int, nGroupProcs);
        if (!(groupHostData[host].groupProcIDOnHost)) {
            ulm_err(("Error: Out of memory\n"));
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        for (int proc = 0; proc < groupHostData[host].nGroupProcIDOnHost;
             proc++) {
            groupHostData[host].groupProcIDOnHost[proc] =
                tmpProcList[proc];
        }

        // set on host root ProcID - this is the local (host) ProcID
        groupHostData[host].hostCommRoot = tmpProcList[0];

        // set designated receiver ProcID for the host
        groupHostData[host].destProc =
            mapGroupProcIDToGlobalProcID[tmpProcList[0]];

        ulm_free(tmpProcList);
    }

    // create list of hostID -> index mappings in Communicator
    int max_hostid = 0;
    for (int host = 0; host < numberOfHostsInGroup; host++) {
        if (groupHostList[host] > max_hostid)
            max_hostid = groupHostList[host];
    }
    groupHostDataLookup = ulm_new(int, max_hostid + 1);
    if (!groupHostDataLookup) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // poison all unused entries to avoid weirdness and hopefully fall over...
    for (int host = 0; host <= max_hostid; host++) {
        groupHostDataLookup[host] = 0xdeadbeef;
    }
    // now fill in the usable entries
    for (int host = 0; host < numberOfHostsInGroup; host++) {
        groupHostDataLookup[groupHostList[host]] = host;
    }

    // free larger groupHostList now...
    ulm_free(groupHostList);

    // set rankList
    mapGlobalProcIDToGroupProcID = ulm_new(int, nprocs());
    if (!mapGlobalProcIDToGroupProcID) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // loop over all global ranks
    for (int i = 0; i < nprocs(); i++) {
        // first set ProcID not to be in list
        mapGlobalProcIDToGroupProcID[i] = -1;
        // search mapGroupProcIDToGlobalProcID to see if this global ProcID is in communicator
        for (int j = 0; j < groupSize; j++) {
            if (mapGroupProcIDToGlobalProcID[j] == i) {
                // set map
                mapGlobalProcIDToGroupProcID[i] = j;
                break;
            }
        }                       // end j loop
    }                           // end i loop

    // set mapGroupProcIDToHostID array
    mapGroupProcIDToHostID = ulm_new(int, groupSize);
    if (!mapGroupProcIDToHostID) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // loop over processes in group
    for (int proc = 0; proc < groupSize; proc++) {

        int gRank = mapGroupProcIDToGlobalProcID[proc];
        int cumProcs = 0;
        // loop over list of processes per host
        for (int host = 0; host < lampiState.nhosts; host++) {
            cumProcs += lampiState.map_host_to_local_size[host];
            // set host ProcID
            if (cumProcs > gRank) {
                mapGroupProcIDToHostID[proc] = host;
                break;
            }

        }                       // end host loop

    }                           // end proc loop

    // set ProcID
    ProcID = mapGlobalProcIDToGroupProcID[myproc()];

    // set local ProcID
    onHostProcID = 0;
    for (int proc = 0; proc < ProcID; proc++) {
        if (mapGroupProcIDToHostID[proc] == myhost()) {
            onHostProcID++;
        }
    }

    // number of processes in this group on this host
    if (hostIndexInGroup < 0) {
        onHostGroupSize = 0;
    } else {
        onHostGroupSize =
            groupHostData[hostIndexInGroup].nGroupProcIDOnHost;
    }

    // find the maximum number of procs per host in the group
    maxOnHostGroupSize = 0;
    for (int host = 0; host < numberOfHostsInGroup; host++) {
        if (maxOnHostGroupSize < groupHostData[host].nGroupProcIDOnHost) {
            maxOnHostGroupSize = groupHostData[host].nGroupProcIDOnHost;
        }
    }

    // map of group ProcID to on-host group ProcID
    mapGroupProcIDToOnHostProcID = ulm_new(int, groupSize);
    if (!mapGroupProcIDToOnHostProcID) {
        ulm_err(("Error: Out of memory"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // loop over processes in group
    for (int i = 0; i < groupSize; i++) {       // initialize
        mapGroupProcIDToOnHostProcID[i] = -1;
    }
    // loop over processes in group on-host
    for (int i = 0; i < onHostGroupSize; i++) { // set
        int group_procid;
        group_procid =
            groupHostData[hostIndexInGroup].groupProcIDOnHost[i];
        // count group processes less than me and on-host
        mapGroupProcIDToOnHostProcID[group_procid] = 0;
        for (int proc = 0; proc < group_procid; proc++) {
            if (mapGroupProcIDToHostID[proc] == myhost()) {
                mapGroupProcIDToOnHostProcID[group_procid]++;
            }
        }
    }

    /* set up binary interhostTree - for gather and scatter routines */
    int treeOrder = GATHER_TREE_ORDER;
    int returnValue = setupNaryTree(numberOfHostsInGroup, treeOrder,
                                    &interhostGatherScatterTree);
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }

    /* set up information for inter-host scatter routine */
    scatterDataNodeList =
        (int **) ulm_malloc(sizeof(int *) * numberOfHostsInGroup);
    if (!scatterDataNodeList) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    lenScatterDataNodeList =
        (int *) ulm_malloc(sizeof(int) * numberOfHostsInGroup);
    if (!lenScatterDataNodeList) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    for (int i = 0; i < numberOfHostsInGroup; i++) {
        lenScatterDataNodeList[i] = 0;
        /* figure out the host order in which the data will be stored */
        getScatterNodeOrder(i, scatterDataNodeList[i],
                            lenScatterDataNodeList + i, 0,
                            &interhostGatherScatterTree);
        if (lenScatterDataNodeList[i] > 0) {
            scatterDataNodeList[i] = (int *)
                ulm_malloc(sizeof(int) * lenScatterDataNodeList[i]);
            if (!scatterDataNodeList[i]) {
                ulm_err(("Error: Out of memory\n"));
                return ULM_ERR_OUT_OF_RESOURCE;
            }
            lenScatterDataNodeList[i] = 0;
            getScatterNodeOrder(i, scatterDataNodeList[i],
                                lenScatterDataNodeList + i, 1,
                                &interhostGatherScatterTree);
        }
    }

    /* set up information for inter-host gather routine */
    gatherDataNodeList =
        (int **) ulm_malloc(sizeof(int *) * numberOfHostsInGroup);
    if (!gatherDataNodeList) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    lenGatherDataNodeList =
        (int *) ulm_malloc(sizeof(int) * numberOfHostsInGroup);
    if (!lenGatherDataNodeList) {
        ulm_err(("Error: Out of memory\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    for (int i = 0; i < numberOfHostsInGroup; i++) {
        lenGatherDataNodeList[i] = 0;
        /* figure out the host order in which the data will be stored */
        getGatherNodeOrder(i, gatherDataNodeList[i],
                           lenGatherDataNodeList + i, 0,
                           &interhostGatherScatterTree);
        if (lenGatherDataNodeList[i] > 0) {
            gatherDataNodeList[i] = (int *)
                ulm_malloc(sizeof(int) * lenGatherDataNodeList[i]);
            if (!gatherDataNodeList[i]) {
                ulm_err(("Error: Out of memory\n"));
                return ULM_ERR_OUT_OF_RESOURCE;
            }
            lenGatherDataNodeList[i] = 0;
            getGatherNodeOrder(i, gatherDataNodeList[i],
                               lenGatherDataNodeList + i, 1,
                               &interhostGatherScatterTree);
        }
    }

    dump();

    return ULM_SUCCESS;
}


/*
 * decrement the reference count, and release resources if refCount is 0
 */
int Group::freeGroup()
{
    int returnValue = ULM_SUCCESS;
    int i;

    groupLock.lock();
    refCount--;
    if (refCount == 0) {
        // free maps
        ulm_delete(mapGlobalProcIDToGroupProcID);
        ulm_delete(mapGroupProcIDToGlobalProcID);
        ulm_delete(mapGroupProcIDToOnHostProcID);
        ulm_delete(mapGroupProcIDToHostID);

        // free groupHostData
        for (i = 0; i < numberOfHostsInGroup; i++) {
            ulm_delete(groupHostData[i].groupProcIDOnHost);
        }
        ulm_delete(groupHostData);
        ulm_delete(groupHostDataLookup);

        // free gather/scatter data
        ulm_free(lenScatterDataNodeList);
        ulm_free(lenGatherDataNodeList);
        for (i = 0; i < numberOfHostsInGroup; i++) {
            ulm_free(scatterDataNodeList[i]);
            ulm_free(gatherDataNodeList[i]);
        }
        ulm_free(scatterDataNodeList);
        ulm_free(gatherDataNodeList);

        // free tree data interhostGatherScatterTree
        if (interhostGatherScatterTree.nUpNodes) {
            ulm_free(interhostGatherScatterTree.nUpNodes);
            interhostGatherScatterTree.nUpNodes = 0;
        }
        if (interhostGatherScatterTree.upNodes) {
            ulm_free(interhostGatherScatterTree.upNodes);
            interhostGatherScatterTree.upNodes = 0;
        }
        if (interhostGatherScatterTree.nInPartialLevel) {
            ulm_free(interhostGatherScatterTree.nInPartialLevel);
            interhostGatherScatterTree.nInPartialLevel = 0;
        }
        if (interhostGatherScatterTree.startLevel) {
            ulm_free(interhostGatherScatterTree.startLevel);
            interhostGatherScatterTree.startLevel = 0;
        }
        if (interhostGatherScatterTree.nDownNodes) {
            ulm_free(interhostGatherScatterTree.nDownNodes);
            interhostGatherScatterTree.nDownNodes = 0;
        }
        if (interhostGatherScatterTree.downNodes) {
            for (i = 0; i < interhostGatherScatterTree.nNodes; i++) {
                if (interhostGatherScatterTree.downNodes[i]) {
                    ulm_free(interhostGatherScatterTree.downNodes[i]);
                    interhostGatherScatterTree.downNodes[i] = 0;
                }
            }
            ulm_free(interhostGatherScatterTree.downNodes);
            interhostGatherScatterTree.downNodes = 0;
        }
    }
    // unlock group
    groupLock.unlock();

    return returnValue;
}


/*
 * Utility functions for groups
 */


/*
 * create a new group with the list of global ranks
 */
int setupNewGroupObject(int grpSize, int *listOfRanks, int *groupIndex)
{
    int returnValue = ULM_SUCCESS;

    // get free group element
    returnValue = getGroupElement(groupIndex);
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }

    // fill in group information
    Group *groupPtr = grpPool.groups[(*groupIndex)];
    returnValue = groupPtr->create(grpSize, listOfRanks, *groupIndex);
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }

    return returnValue;
}


/*
 * find available element in grpPool array
 */
int getGroupElement(int *slotIndex)
{
    int grpIndex;
    int i;
    int newLength;
    int nextAvailElement;
    int slotAvail;
    ssize_t lenToAlloc;

    // lock the pool
    grpPool.poolLock.lock();

    slotAvail = grpPool.firstFreeElement;

    if (slotAvail >= grpPool.poolSize) {

        //
        // allocate new groupPool with larger size
        //
        newLength = grpPool.poolSize + nIncreaseGroupArray;
        Group **tmpGroups = ulm_new(Group *, newLength);
        if (!tmpGroups) {
            ulm_exit((-1,
                      "Error: Out of memory. "
                      "Unable to allocate space for group objects\n"));
        }
        // copy information from old groupPool to new copy
        for (grpIndex = 0; grpIndex < grpPool.poolSize; grpIndex++) {
            tmpGroups[grpIndex] = grpPool.groups[grpIndex];
        }                       // end grpIndex loop
        // initialize new elements
        for (grpIndex = grpPool.poolSize; grpIndex < newLength; grpIndex++) {
            tmpGroups[grpIndex] = 0;
        }                       // end grpIndex loop

        // free old element
        if (grpPool.poolSize > 0) {
            ulm_delete(grpPool.groups);
        }
        // reset groupPool
        grpPool.groups = tmpGroups;
        grpPool.poolSize = newLength;

    }                           // end finding slot

    // allocate Group element
    lenToAlloc =
        (((sizeof(Group) - 1) / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
    grpPool.groups[slotAvail] = (Group *) ulm_malloc(lenToAlloc);
    if (!grpPool.groups[slotAvail]) {
        ulm_err(("Error: Out of memory\n"));
        grpPool.poolLock.unlock();
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // run constructor
    new(grpPool.groups[slotAvail]) Group;

    // set element to be used
    *slotIndex = slotAvail;

    // find next available slot
    nextAvailElement = grpPool.poolSize;
    for (i = slotAvail; i < grpPool.poolSize; i++) {
        if (grpPool.groups[i] == 0) {
            nextAvailElement = i;
            break;
        }
    }
    grpPool.firstFreeElement = nextAvailElement;

    // unlock pool
    grpPool.poolLock.unlock();

    return ULM_SUCCESS;
}
