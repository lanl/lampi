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

#ifndef _PATHCONTAINER_HEADER
#define _PATHCONTAINER_HEADER

#include "util/Lock.h"
#include "internal/constants.h"
#include "queue/globals.h"
#include "path/common/path.h"

// enumeration used for getInfo/setInfo for pathContainers
enum pathContainerInfoType {
    MAX_PATHS_ALLOWED,                  /* the number of paths that
                                           can be allocated from this
                                           pathContainer */
    MAX_SIZE_IN_BYTES_FOR_PATH_OBJECT   /* size in bytes of internal
                                           pathContainer storage used
                                           for a path object */
};

enum {
    MAX_PATHS = 8,
    MAX_PATH_OBJECT_SIZE_IN_BYTES = 2048        /* or whatever */
};

// this object can live in anonymous shared or process private memory
// read access is not protected, so returned values from
// path() and allPaths() can be inconsistent as to whether
// or not the paths are still active...
class pathContainer_t {
private:
    enum pathStat {
        UNALLOCATED,
        INACTIVE,
        ACTIVE
    };

    // first version will store a MAX_PATHS array of bitmaps per box/host
    //!!!! this will only work if every path to a host can reach all processes
    // on that host
    unsigned long long mapStorage[ULM_MAX_HOSTS];
    // bit mask values for access into mapStorage
    unsigned long long masks[MAX_PATHS];
    // this is the storage/memory for the path objects themselves
    char pathStorage[MAX_PATH_OBJECT_SIZE_IN_BYTES * MAX_PATHS];
    // array of pointers to paths in pathStorage, indexed via path handle
    BasePath_t *pathArray[MAX_PATHS];
    // status of path objects, indexed via path handle
    pathStat pathStatus[MAX_PATHS];
    // modify/write lock for this pathContainer
    Locks lock;
    // number of allocated paths
    int numAllocated;

public:
    // default constructor
    pathContainer_t()
        {
            char *tmp = pathStorage;
            for (int i = 0; i < MAX_PATHS; i++) {
                masks[i] = (unsigned long long)(1 << i);
                pathArray[i] = (BasePath_t *)tmp;
                pathStatus[i] = UNALLOCATED;
                tmp += MAX_PATH_OBJECT_SIZE_IN_BYTES;
            }
            lock.init();
            numAllocated = 0;
        }

    // default destructor
    ~pathContainer_t() {}

    // return up to maxPaths pointers to active paths to get to all
    // globalDestProcessIDs in the destination array of destArrayCount entries;
    // actual number of paths in pathArrayArg is the return value of this method
    // NOTE: there is no locking so the list is potentially inconsistent
    int paths(int *globalDestProcessIDArray, int destArrayCount,
              BasePath_t **pathArrayArg, int maxPaths)
        {
            int numCopied = 0;
            for (int j = 0; j < destArrayCount; j++) {
                long int host = global_proc_to_host(globalDestProcessIDArray[j]);
                unsigned long long currentPathMap = mapStorage[host];
                if (j == 0) {
                    // first time through, just record all paths that can reach this host...
                    for (int i = 0; i < numAllocated; i++) {
                        if ((masks[i] & currentPathMap) && (numCopied < maxPaths) && (pathStatus[i] == ACTIVE)) {
                            pathArrayArg[numCopied++] = pathArray[i];
                        }
                    }
                }
                else {
                    // additional destination -- reset any path entry that can not reach this host...
                    for (int i = 0; i < numCopied; i++) {
                        if (pathArrayArg[i] && !(masks[pathArrayArg[i]->getHandle()] & currentPathMap))
                            pathArrayArg[i] = 0;
                    }
                }
            }
            // remove any possible null pointers if destArrayCount > 1
            if (destArrayCount > 1) {
                int k = 0;
                for (int i = 0; i < numCopied; i++) {
                    if (pathArrayArg[i]) {
                        if (k == i)
                            k++;
                        else
                            pathArrayArg[k++] = pathArrayArg[i];
                    }
                }
                numCopied = k;
            }

            return numCopied;
        }

    // return array containing all of the active paths known by this container
    // the actual number of paths in pathArray is the return value of this method
    // NOTE: there is no locking so the list is potentially inconsistent
    int allPaths(BasePath_t **pathArrayArg, int maxPaths)
        {
            int numCopied = 0;
            for (int i = 0; i < numAllocated; i++) {
                if ((numCopied < maxPaths) && (pathStatus[i] == ACTIVE)) {
                    pathArrayArg[numCopied++] = pathArray[i];
                }
            }
            return numCopied;
        }

    // return whether or not this path can be used to reach
    //   remote process remoteProc
    int canUsePath(int remoteProc, int pathIndex)
        {
		int returnValue=0;
		/* check array bounds */
		if( pathIndex >= numAllocated) {
			return returnValue;
		}

		if( (pathArray[pathIndex]->canReach(remoteProc))
				&& (pathStatus[pathIndex] == ACTIVE) )
		{
			returnValue=1;
		}
		return returnValue;
        }

    // returns a pointer to MAX_PATH_OBJECT_SIZE_IN_BYTES bytes
    // of shared memory (or 0 on failure), to instantiate the path object,
    // and returns a pathHandle to the new object for activate(), etc.
    void *add(int *pathHandle)
        {
            // on failure, pathHandle points to -1 and returnValue is 0
            void *returnValue = 0;
            *pathHandle = -1;
            lock.lock();
            for (int i = 0; i < MAX_PATHS; i++) {
                if (pathStatus[i] == UNALLOCATED) {
                    pathStatus[i] = INACTIVE;
                    *pathHandle = i;
                    returnValue = pathArray[i];
                    numAllocated++;
                    break;
                }
            }
            lock.unlock();
            return returnValue;
        }

    // calls path::bindToContainer() and uses path::canReach() to set up internal mapping
    // of destination process to list of paths, finally calls path::activate()
    void activate(int pathHandle)
        {
            int processes = nprocs();
            BasePath_t *pathPtr = pathArray[pathHandle];
            pathPtr->bindToContainer(this, pathHandle);
            lock.lock();
            for (int i = 0; i < processes; i++) {
                int host = global_proc_to_host(i);
                if (pathPtr->canReach(i)) {
                    // this is overkill, as many processes can map to
                    // a host, we could use groupHostData for collective
                    // communications to simply map the destProc (global process
                    // ID) for every host (using hostID)...and therefore
                    // reduce the setup/initialization work
                    mapStorage[host] |= masks[pathHandle];
                }
                else {
                    mapStorage[host] &= ~masks[pathHandle];
                }
            }
            pathPtr->activate();
            pathStatus[pathHandle] = ACTIVE;
            lock.unlock();
        }

    // update the map of available destinations via a given path
    void update(int pathHandle) {
        int processes = nprocs();
        BasePath_t *pathPtr = pathArray[pathHandle];
        lock.lock();
        for (int i = 0; i < processes; i++) {
            int host = global_proc_to_host(i);
            if (pathPtr->canReach(i)) {
                mapStorage[host] |= masks[pathHandle];
            }
            else {
                mapStorage[host] &= ~masks[pathHandle];
            }
        }
        lock.unlock();
    }

    // disables any mapping of destination processes to
    // this path, and calls path::deactivate()
    void deactivate(int pathHandle) {
        lock.lock();
        pathStatus[pathHandle] = INACTIVE;
        pathArray[pathHandle]->deactivate();
        lock.unlock();
    }

    //!!!! DANGEROUS - someone may use this path object...
    // call path destructor (assumes placement new was called to
    // create the path) and free slot in container
    // for this path
    /*
      void remove(int pathHandle) {
      lock.lock();
      pathArray[pathHandle]->deactivate();
      pathStatus[pathHandle] = INACTIVE;
      delete pathArray[pathHandle];
      pathStatus[pathHandle] = UNALLOCATED;
      lock.unlock();
      }
    */

    // set information for this pathContainer and potentially the path object at pathHandle
    // depending upon the information
    bool setInfo(pathContainerInfoType info, int pathHandle, void *addr, int addrSize, int *errorCode) {
        *errorCode = ULM_SUCCESS;
        return true;
    }

    // get information for this pathContainer and potentially the path object at pathHandle
    // depending upon the information
    bool getInfo(pathContainerInfoType info, int pathHandle, void *addr, int addrSize, int *errorCode) {
        int *intResult = (int *)addr;
        *errorCode = ULM_SUCCESS;
        if (!addr) {
            *errorCode = ULM_ERROR;
            return false;
        }
        switch (info) {
        case MAX_PATHS_ALLOWED:
            if (addrSize < (int) sizeof(int)) {
                *errorCode = ULM_ERROR;
                return false;
            }
            *intResult = MAX_PATHS;
            break;
        case MAX_SIZE_IN_BYTES_FOR_PATH_OBJECT:
            if (addrSize < (int) sizeof(int)) {
                *errorCode = ULM_ERROR;
                return false;
            }
            *intResult = MAX_PATH_OBJECT_SIZE_IN_BYTES;
            break;
        default:
            *errorCode = ULM_ERROR;
            return false;
        }
        return true;
    }
};

#endif
