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



#ifndef _GROUPOBJECT
#define _GROUPOBJECT

#include "util/Lock.h"
#include "internal/collective.h"

typedef struct {
    int *groupProcIDOnHost; // list of ranks in this context on a host
    int nGroupProcIDOnHost; // number of ranks on host in this context
    int hostID;             // host ID number
    int hostCommRoot;       // smallest ProcID in context on host
    int destProc;           // global ProcID of proc that will get frags for coll. comm.
} ulm_host_info_t;


class Group {
public:
    Locks groupLock;                    // lock to control group access
    int *mapGlobalProcIDToGroupProcID;  // map: global procid -> group procid
    int *mapGroupProcIDToGlobalProcID;  // map: group procid -> global procid
    int *mapGroupProcIDToHostID;        // map: group procid -> global hostid
    int *mapGroupProcIDToOnHostProcID;  // map: group procid -> on-host group procid
    int *groupHostDataLookup;           // map: global hostid -> groupHostData index
    ulm_host_info_t *groupHostData;     // group host data
    int ProcID;                         // ProcID in group
    int groupSize;
    int hostIndexInGroup;               // host index in list of hosts
    int numberOfHostsInGroup;           // number of hosts in group
    int onHostProcID;                   // local ProcID in group
    int onHostGroupSize;                // number of processes in this group on this host
    int maxOnHostGroupSize;             // maximum number of processes on any host in this group
    int refCount;                       // number of times this group is referenced in a comm
    int groupID;
    ulm_tree_t interhostGatherScatterTree; // used in some collective algorithms to setup interhost
    //   communications patterns.
    // list of data for destination nodes per node for scatter routine
    //   since a fan-out routine is used, a sending node may have data for
    //   a node to which it does not directly send data
    int **scatterDataNodeList;
    int *lenScatterDataNodeList;

    // list of data for destination nodes per node for gather routine
    //   since a fan-in routine is used, a sending node may have data for
    //   a node to which it does not directly send data
    int **gatherDataNodeList;
    int *lenGatherDataNodeList;

    // constructor
    Group();

    // destructor
    ~Group();

    // copy constructor
    Group(Group &dupGroup);

    // initialization function
    int create(int grpSize, int *listOfRanks, int groupID);

    // free function
    int freeGroup();

    // dump function for debugging
    void dump();

    // accessor methods
    int getSize()
        {
            return groupSize;
        }

    int getRank()
        {
            return ProcID;
        }

    int getNumOfOnHostGroups()
        {
            return numberOfHostsInGroup;
        }

    void incrementRefCount(int n = 1)
        {
            groupLock.lock();
            refCount += n;
            groupLock.unlock();
        }
};

// pool of Group and control information needed to manage it.
typedef struct _groupPool {
    Locks poolLock;             // lock to control access to pool
    Group **groups;       // array of elements
    int eleInUse;               // number of elements in use
    int firstFreeElement;       // first element available
    int poolSize;               // size of pool
} groupPool;

#endif // _GROUPOBJECT
