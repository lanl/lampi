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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/pmo.h>
#include <sys/mman.h>
#include <invent.h>

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/system.h"
#include "internal/types.h"
#include "os/IRIX/acquire.h"
#include "os/IRIX/MD_acquireGlobals.h"

// variable indicating if memory placement succeeded.
bool placementSucceeded = false;

//! allocate resrouce affinity list, and setup mlds, mld set, policy modules,
//!   and allocate atomic hardware resources.
bool acquire::init_base(int p, bool cpu2node, pmo_handle_t * mldlist,
                        int numraffs, raff_info_t * rafflist)
{
    int i;
    //! sanity check on number of cpu's
    if (!(p > 0)) {
        _emit_error_msg_and_exit();
    }

    numcpus = p;
    cpunum = ulm_new(int, numcpus);
    if (!cpunum) {
        _emit_error_msg_and_exit();
    }
    //!
    //! resource affinity is to be used
    //!
    if (useResourceAffinity) {

        if (useDefaultAffinity) {
            //!
            //! let the os generate resource affinity list
            //!
            if (cpu2node) {
                //! 1 cpu per node
                nummlds = numcpus;
                mldnum = ulm_new(int, nummlds);
                if (!mldnum) {
                    ulm_err(("Error: Out of memory\n"));
                    _emit_error_msg_and_exit();
                }
                //! set mld number and cpu number (0 always) for process i
                for (i = 0; i < numcpus; i++) {
                    mldnum[i] = i;
                    cpunum[i] = 0;
                }
                //! setup mlds
                mld = ulm_new(pmo_handle_t, nummlds);
                if (!mld) {
                    ulm_err(("Error: Out of memory\n"));
                    _emit_error_msg_and_exit();
                }
                for (i = 0; i < nummlds; i++) {
                    mld[i] = mld_create(0, 1);
                }
                //! setup mld set
                mldset = mldset_create(mld, nummlds);
                if (mldset == -1) {
                    ulm_err(("Error: Creating msdset\n"));
                    _emit_error_msg_and_exit();
                }
            } else {
                //! 2 cpu per node
                nummlds = ((numcpus - 1) / 2) + 1;
                mldnum = ulm_new(int, numcpus);
                if (!mldnum) {
                    ulm_err(("Error: Out of memory\n"));
                    _emit_error_msg_and_exit();
                }
                //! set mld number and cpu number (0 always) for process i
                for (i = 0; i < numcpus; i++) {
                    //! set mld number and cpu number for process i
                    mldnum[i] = i / 2;
                    cpunum[i] = i % 2;
                }
                //! setup mlds
                mld = ulm_new(pmo_handle_t, nummlds);
                if (!mld) {
                    ulm_err(("Error: Out of memory\n"));
                    _emit_error_msg_and_exit();
                }
                for (i = 0; i < nummlds; i++) {
                    mld[i] = mld_create(0, 1);
                }
                //! setup mld set
                mldset = mldset_create(mld, nummlds);
                if (mldset == -1) {
                    ulm_err(("Error: Creating mldset\n"));
                    _emit_error_msg_and_exit();
                }
            }

            //! place mldset
            int RetVal =
                mldset_place(mldset, TOPOLOGY_FREE, NULL, 0,
                             RQMODE_MANDATORY);
            if (RetVal < 0) {
                ulm_warn(("Warning: Could not place mldset\n"));
                if (affinityMandatory) {
                    ulm_err(("Error: exiting\n"));
                    _emit_error_msg_and_exit();
                }
            } else {
                placementSucceeded = true;
            }

        } else {                // useDefaultAffinity

            //!
            //! use input resource affinity list
            //!

            //! sanity check
            if (numcpus != numraffs) {
                ulm_err(("Error: numcpus (%d) != numraffs (%d)\n",
                         numcpus, numraffs));
                _emit_error_msg_and_exit();
            }
            nummlds = numcpus;
            for (i = 0; i < numcpus; i++) {

                mldnum[i] = i;
                raff_info_t *raff = &(rafflist[i]);
                if (!(raff->restype == RAFFIDT_NAME)) {
                    _emit_error_msg_and_exit();
                }
                //! figure out if this is cpu a or cpu b
                char *cpuPtr = strstr("cpu", (char *) (raff->resource));
                if (cpuPtr == NULL) {
                    ulm_err(("Error: Can't find cpu string\n"));
                    _emit_error_msg_and_exit();
                }
                int len = cpuPtr - (char *) (raff->resource);
                len = len + 5;
                if (len > raff->reslen) {
                    ulm_err(("Error: Can't find which cpu in string (%s)\n",
                             raff->resource));
                    _emit_error_msg_and_exit();
                }

                if (*(cpuPtr + 4) == 'a') {
                    cpunum[i] = 0;
                } else {
                    cpunum[i] = 1;
                }
            }                   // end loop over resource affinity list
            //! setup mlds
            mld = ulm_new(pmo_handle_t, nummlds);
            if (!mld) {
                ulm_err(("Error: Allocating mld. nummlds = %d\n",
                         nummlds));
                _emit_error_msg_and_exit();
            }
            for (i = 0; i < nummlds; i++) {
                mld[i] = mld_create(0, 1);
            }
            //! setup mld set
            mldset = mldset_create(mld, nummlds);
            if (mldset == -1) {
                ulm_err(("Error: Creating mldset\n"));
                _emit_error_msg_and_exit();
            }
            //! place mld
            int RetVal = mldset_place(mldset, TOPOLOGY_PHYSNODES,
                                      rafflist, numraffs,
                                      RQMODE_MANDATORY);
            if (RetVal < 0) {
                ulm_warn(("Warning: Could not place mldset\n"));
                if (affinityMandatory) {
                    _emit_error_msg_and_exit();
                }
            } else {
                placementSucceeded = true;
            }

        }                       // useDefaultAffinity
    }                           // end useResourceAffinity segment

    // Get the node list for these cpus.
    // the resrouce affinity list of cpus actually used is still available here.
    nCpuNodes = 0;
    if (placementSucceeded) {
        int errorCode =
            nodeListFromCpuList(numcpus, &nCpuNodes, &nodeRaffList);
        if (errorCode != ULM_SUCCESS) {
            _emit_error_msg_and_exit();
        }
    }
    // allocate atomic fetch and op hardware, if useO2kFetchOpHardware is set
    if (nCpuNodes > 0)
        useO2kFetchOpHardware = true;
    if (useO2kFetchOpHardware) {
        allocO2kFetchOpbarrierPools();
        // for some reason were not able to get the resource
        if (hwBarrier.nPools <= 0)
            useO2kFetchOpHardware = false;
    }

    //! setup policy module
    numpms = 0;
    if (placementSucceeded) {
        pm = ulm_new(int, numcpus);
        if (!pm) {
            _emit_error_msg_and_exit();
        }
        for (int cpu = 0; cpu < numcpus; cpu++) {
            policy_set_t ps;
            // default values for policy set
            pm_filldefault(&ps);
            // fix the placement of the memory
            ps.placement_policy_name = "PlacementFixed";
            ps.placement_policy_args = (void *) mld[mldnum[cpu]];
            pm[cpu] = pm_create(&ps);
            if (pm[cpu] == -1) {
                ulm_err(("Error: pm_create\n"));
                _emit_error_msg_and_exit();
            }
        }

        //! set the number of policy modules
        numpms = numcpus;
    }

    return (true);
}


//
// attach a policy to an address range
//
// attach the entire address range to one mld
bool acquire::set_policy(void *ptr, int len, int cpu)
{
    // check to see if policy should be attached
    if (!placementSucceeded) {
        return (true);
    }
    // attach the policy to the ENTIRE virtual address range
    int error = pm_attach(pm[cpu], ptr, len);
    if (error < 0) {
        return (false);
    }
    return (true);
}

//! constructor
acquire::acquire(int p, int actual_numres, resource_info * ri)
{
    int i, numres = actual_numres;
    raff_info_t *rafflist;
    free_mld = false;
    if (acquire_resource(numres, ri) == false) {
        cerr << "Warning: could not acquire all requested resources" <<
            endl;
    }

    for (i = 0; i < actual_numres; i++) {
        if (ri[i].get_type() == R_HIPPI) {
            numres++;
        }
    }
#ifdef _BLAH_
    cout << "numres = " << numres << " actual = " << actual_numres << endl;
#endif
    rafflist = ulm_new(raff_info_t, numres);
    // build the rafflist
    for (i = 0; i < actual_numres; i++) {
        if (ri[i].get_type() == R_HIPPI) {
            char *cursor;
            rafflist[i].radius = 1;
            // total hack!  hippi boards in xio slot 5 connected to 2 nodes
            rafflist[i].resource =
                ulm_new(char, strlen(ri[i].get_path()) + 1);
            // /hw/module/XX/slot/
#ifdef _PURIFY_
            // purify doesn't seem to understand or "appreciate" strncpy()
            //  or it can't follow this yucky pointer stuff i'm doing
            strcpy((char *) rafflist[i].resource, ri[i].get_path());
#else
            strncpy((char *) rafflist[i].resource, ri[i].get_path(), 19);
#endif
            cursor = ((char *) rafflist[i].resource) + 18;
            if (*cursor == '/') {
                cursor++;
            }
            strcpy(cursor, "n1/node");
            rafflist[i].reslen = strlen((char *) rafflist[i].resource) + 1;

            // now do everything for the "other" node
            rafflist[i + actual_numres].radius = 1;
            rafflist[i + actual_numres].resource =
                ulm_new(char, strlen((char *) rafflist[i].resource) + 1);
            strcpy((char *) rafflist[i + actual_numres].resource,
                   (char *) rafflist[i].resource);
            rafflist[i + actual_numres].reslen = rafflist[i].reslen;
            ((char *) rafflist[i + actual_numres].resource)[rafflist[i].
                                                            reslen - 1 -
                                                            6] = '3';
            rafflist[i + actual_numres].restype = RAFFIDT_NAME;
            rafflist[i + actual_numres].attr = RAFFATTR_ATTRACTION;

        } else {
            rafflist[i].radius = 0;
            rafflist[i].resource = ri[i].get_path();
            rafflist[i].reslen = strlen((char *) rafflist[i].resource) + 1;
#ifdef _BLAH_
            cerr << "adding " << ri[i].
                get_path() << " length " << rafflist[i].reslen << endl;
#endif                          // _BLAH_
        }
        rafflist[i].restype = RAFFIDT_NAME;
        rafflist[i].attr = RAFFATTR_ATTRACTION;
    }
#ifdef _BLAH_
    for (i = 0; i < numres; i++) {
        cout << "resource name:" << (char *) rafflist[i].resource << endl;
    }
#endif
    // use O2k atmoic fetch and op hardware
    useO2kFetchOpHardware = true;

    // set cpu per node usage
    bool cpuUsage = false;
    if (nCpPNode == 1) {
        cpuUsage = true;
        nCpuNodes = 1;
    } else {
        nCpuNodes = 2;
    }
    // set resource affinity flags
    useResourceAffinity = useRsrcAffinity;
    useDefaultAffinity = useDfltAffinity;
    affinityMandatory = affinMandatory;

    if (init_base(p, cpuUsage, NULL, numres, rafflist)) {
        for (i = 0; i < numres; i++) {
            if (rafflist[i].radius > 0) {
                ulm_delete(rafflist[i].resource);
            }
        }
        ulm_delete(rafflist);
        MI_init(p);
    }
}

int acquire::allocO2kFetchOpbarrierPools()
{
    //!
    //! set the number of pools
    //!
    hwBarrier.nPools = nCpuNodes;

    //!
    //! check to make sure nCpuNodes is positive
    //!
    if (hwBarrier.nPools <= 0) {
        return ULM_SUCCESS;
    }
    //!
    //!  determine how many pages to allocate (the granularity of the
    //!    per node allocation is 1 page)
    //!
    int nPages = hwBarrier.nPools;

    //!
    //! allocate nElementsInPool
    //!
    hwBarrier.nElementsInPool =
        (int *) ulm_malloc(sizeof(int) * hwBarrier.nPools);
    if (!(hwBarrier.nElementsInPool)) {
        ulm_exit((-1, "Aborting\n"));
    }
    //!
    //! allocate lastPoolUsed
    //!
    hwBarrier.lastPoolUsed = (int *) SharedMemoryPools.getMemorySegment
        (sizeof(int), CACHE_ALIGNMENT);
    if (!(hwBarrier.lastPoolUsed)) {
        ulm_exit((-1, "Aborting\n"));
    }
    *(hwBarrier.lastPoolUsed) = 0;

    /* allocate and initialize lock */
    hwBarrier.Lock = (Locks *) SharedMemoryPools.getMemorySegment
        (sizeof(Locks), CACHE_ALIGNMENT);
    if (!(hwBarrier.lastPoolUsed)) {
        ulm_exit((-1, "Aborting\n"));
    }
    hwBarrier.Lock->init();

    //!
    //! allocate pool
    //!
    hwBarrier.pool = (barrierFetchOpDataCtlData **)
        ulm_malloc(sizeof(barrierFetchOpDataCtlData *) * hwBarrier.nPools);
    if (!(hwBarrier.pool)) {
        ulm_exit((-1, "Aborting\n"));
    }

    //! set up policy modules for this memory
    pmo_handle_t *barrierMLD =
        (pmo_handle_t *) ulm_malloc(sizeof(pmo_handle_t) *
                                    hwBarrier.nPools);
    if (!barrierMLD) {
        ulm_exit((-1, "Aborting\n"));
    }
    //!  create mld's
    for (int i = 0; i < hwBarrier.nPools; i++) {
        barrierMLD[i] = mld_create(0, 1);
        //! create no pools if can't create handle
        if (barrierMLD[i] == -1) {
            hwBarrier.nPools = 0;
            free(barrierMLD);
            free(hwBarrier.nElementsInPool);
            free(hwBarrier.pool);
            return ULM_SUCCESS;
        }
    }

    //!  create mld set
    pmo_handle_t barrierMLDset =
        mldset_create(barrierMLD, hwBarrier.nPools);
    //! create no pools if can't create mld set
    if (barrierMLDset == -1) {
        hwBarrier.nPools = 0;
        free(barrierMLD);
        free(hwBarrier.nElementsInPool);
        free(hwBarrier.pool);
        return ULM_SUCCESS;
    }
    //! allocate shared memory
    long long memory = nPages * getpagesize();
    long long *memPtr = (long long *) alloc_mem(memory, MAP_SHARED);
    if (!memPtr) {
        hwBarrier.nPools = 0;
        free(barrierMLD);
        free(hwBarrier.nElementsInPool);
        free(hwBarrier.pool);
        return ULM_SUCCESS;
    }
    //!  place mld
    int returnCode = mldset_place(barrierMLDset, TOPOLOGY_PHYSNODES,
                                  nodeRaffList, nCpuNodes,
                                  RQMODE_MANDATORY);
    if (returnCode == -1) {
        hwBarrier.nPools = 0;
        free(barrierMLD);
        free(hwBarrier.nElementsInPool);
        free(hwBarrier.pool);
        munmap(memPtr, memory);
        return ULM_SUCCESS;
    }
    //!  use syssgi call to setup the memory
    returnCode = syssgi(SGI_FETCHOP_SETUP, memPtr, memory);
    if (returnCode < 0) {
        // no hardware support for fetchop
        hwBarrier.nPools = 0;
        free(barrierMLD);
        free(hwBarrier.nElementsInPool);
        free(hwBarrier.pool);
        munmap(memPtr, memory);
        return ULM_SUCCESS;
    }
    //!  pin the memory
    returnCode = mpin(memPtr, memory);
    if (returnCode < 0) {
        hwBarrier.nPools = 0;
        free(barrierMLD);
        free(hwBarrier.nElementsInPool);
        free(hwBarrier.pool);
        munmap(memPtr, memory);
        return ULM_SUCCESS;
    }
    //! free the barrier mld
    free(barrierMLD);

    //!
    //! setup the memory pools
    //!
    //! 1 page per hub assumed
    assert(nPages == hwBarrier.nPools);
    long long sizeOfFetchOpRegion = CACHE_ALIGNMENT;
    long long nElements = getpagesize() / sizeOfFetchOpRegion;
    for (int pl = 0; pl < hwBarrier.nPools; pl++) {
        hwBarrier.nElementsInPool[pl] = nElements;
        hwBarrier.pool[pl] = (barrierFetchOpDataCtlData *)
            SharedMemoryPools.getMemorySegment(nElements *
                                               sizeof
                                               (barrierFetchOpDataCtlData),
                                               CACHE_ALIGNMENT);
        if (!(hwBarrier.pool[pl])) {
            ulm_exit((-1, "Aborting\n"));
        }
        long long *fetchopPtr =
            (long long *) ((char *) memPtr + pl * getpagesize());
        for (int ele = 0; ele < nElements; ele++) {
            // allocate the barrierData element out of process private memory
            (hwBarrier.pool[pl][ele]).barrierData =
                (barrierFetchOpData *)
                ulm_malloc(sizeof(barrierFetchOpData));
            if (!(hwBarrier.pool[pl][ele].barrierData)) {
                ulm_exit((-1, "Aborting\n"));
            }
            // mark element as available
            (hwBarrier.pool[pl][ele]).inUse = false;
            // set the pointer to the fetchop variable
            hwBarrier.pool[pl][ele].barrierData->fetchOpVar = fetchopPtr;
            fetchopPtr =
                (long long *) ((char *) fetchopPtr + sizeOfFetchOpRegion);
        }                       // end element loop
    }                           // end pool loop

    return ULM_SUCCESS;
}

// function to extract node list from the cpu list
//
// This routine takes in a cpu resource affinity list, computes
//   the number of nodes in use, and returns this number.
//   A rafflist for the nodes is created.
//

int acquire::nodeListFromCpuList(int nCpus, int *nNodes,
                                 raff_info_t ** nodeRaffList)
{
    // compute number of nodes
    *nNodes = 0;
    // simple sort and compare - the list is short
    //  loop over all cpus in list
    for (int cpu = 0; cpu < nCpus; cpu++) {
        // extract node name
        char devName[1024];
        int lenNodeName = 1024;
        dev_t devT = __mld_to_node(mld[mldnum[cpu]]);
        char *name = dev_to_devname(devT, &(devName[0]), &lenNodeName);

        if (name == NULL)
            return ULM_ERR_BAD_PARAM;

        // check against previous cpus in list
        bool Unique = true;
        for (int cpuPrev = 0; cpuPrev < cpu; cpuPrev++) {
            // extact node name
            char devNm[1024];
            int lenNodeNm = 1024;
            dev_t devT = __mld_to_node(mld[mldnum[cpuPrev]]);
            char *tmpName = dev_to_devname(devT, &(devNm[0]), &lenNodeNm);
            //
            // if unique, increment counter
            //
            int lenSame = strncmp(tmpName, name, lenNodeName);
            if (lenSame == 0) {
                Unique = false;
                break;
            }

        }                       // end cpuPrev loop

        // increment count if this is the first occurence of the node
        // in the list
        if (Unique) {
            (*nNodes)++;
        }

    }                           // end loop over cpus

    // allocate nodeRaffList
    *nodeRaffList = ulm_new(raff_info_t, *nNodes);
    if (!(*nodeRaffList)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // fill in resrouce affinity list
    int nodeNumber = 0;
    // simple sort and compare - the list is short
    //  loop over all cpus in list
    for (int cpu = 0; cpu < nCpus; cpu++) {
        // extract node name
        char devName[1024];
        int lenNodeName = 1024;
        dev_t devT = __mld_to_node(mld[mldnum[cpu]]);
        char *name = dev_to_devname(devT, &(devName[0]), &lenNodeName);
        if (name == NULL) {
            return ULM_ERR_BAD_PARAM;
        }
        // check against previous cpus in list
        bool Unique = true;
        for (int cpuPrev = 0; cpuPrev < cpu; cpuPrev++) {
            // extact node name
            //
            char devNm[1024];
            int lenNodeNm = 1024;
            dev_t devT = __mld_to_node(mld[mldnum[cpuPrev]]);
            char *tmpName = dev_to_devname(devT, &(devNm[0]), &lenNodeNm);
            //
            // if unique, increment counter
            //
            int lenSame = strncmp(tmpName, name, lenNodeName);

            if (lenSame == 0) {
                Unique = false;
                break;
            }

        }                       // end cpuPrev loop

        // increment count if this is the first occurence of the node in the list
        if (Unique) {
            // fill in information
            (*nodeRaffList)[nodeNumber].radius = 1;

            (*nodeRaffList)[nodeNumber].resource =
                new char[lenNodeName + 1];
            if (!((*nodeRaffList)[nodeNumber].resource)) {
                return ULM_ERR_OUT_OF_RESOURCE;
            }
            strncpy((char *) (*nodeRaffList)[nodeNumber].resource, name,
                    lenNodeName);
            char *last =
                (char *) ((*nodeRaffList)[nodeNumber].resource) +
                lenNodeName;
            *last = '\0';

            (*nodeRaffList)[nodeNumber].reslen = lenNodeName;

            (*nodeRaffList)[nodeNumber].restype = RAFFIDT_NAME;

            (*nodeRaffList)[nodeNumber].reslen = RAFFATTR_ATTRACTION;

            // increment node count
            nodeNumber++;
        }

    }                           // end loop over cpus

    return ULM_SUCCESS;
}
