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



#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <errno.h>
#include <iostream.h>
#include <stdio.h>
#include <string.h>

#include "client/ULMClient.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/types.h"
#include "os/IRIX/MD_request_include.h"
#ifdef _USE_TOPOLOGY_
#include "topology.h"
#endif

//
// MD_request.h MUST define:
//  - an enumerated type called resource_type of allocatable and
//    other specifiable resources:
//         - the first element should be allocatable and have a
//           value of 0
//         - the elements following and up to "R_NUM_RESOURCES"
//           should be allocatable
//         - the elements following and up to "R_TOTAL_RESOURCES"
//           should be other specifiable resources
//  - a type called constraint_info used to describe constraints
//  - a type called resource_info used to access info about the
//    allocated resources by the other MD routines and by the
//    calling routine
//

#include "os/IRIX/MD_request.h"

//
// class: request
//
// request a virtual assignment of hardware resources
//
// public:
//   int get_max_resource(resource_type)
//   resource_info *get_resource(resource_type)
//   int get_num_resource(resource_type)
//   void add_constraint(resource_type, int)
//   void add_constraint(topology *t)  /*** not use ***/
//   void add_constraint(resource_type, int, constraint *)
//   void map_resources()
//
// NOTES: the public interface is MACHINE INDEPENDENT
//        the private interface is MACHINE DEPENDENT
//
// we'll see if this structure actually works for more than 2 platforms
//
class
request {
public:
    request() {
        int i;
        for (i = 0; i < R_NUM_RESOURCES; i++) {
            rreq[i] = 0;
            allocated_resources[i] = 0;
            resources[i] = NULL;
        }
        MD_init_resource_info();
    }
    ~request() {
        MD_destructor();
    }

    //
    // get the maximum available for resource rt
    //
    inline int
        get_max_resource(resource_type rt) {
        if (!(rt < R_NUM_RESOURCES)) {
            ulm_exit(("Aborting\n"));
        }
        return max_resources[rt];
    }

    inline int
        get_num_resource(resource_type rt) {
        if (!(rt < R_NUM_RESOURCES)) {
            ulm_exit(("Aborting\n"));
        }
        return(rreq[rt]);
    }

    //
    // get list of resources of type rt mapped by call to map_resources()
    //
    inline resource_info *
        get_resource(resource_type rt) {
        if (!(rt < R_NUM_RESOURCES)) {
            ulm_exit(("Aborting\n"));
        }
        return resources[rt];
    }

    //
    // add contraints to the mapping
    //
    inline void
        add_constraint(resource_type rt, int n) {
        if (!((rt < R_TOTAL_RESOURCES) && (rt != R_NUM_RESOURCES))) {
            ulm_exit(("Aborting\n"));
        }
        if (!(n <= max_resources[rt])) {
            ulm_exit(("Aborting\n"));
        }
        /***
            if (rreq[rt] != 0) {
            delete resources[rt];
            }
        ***/
        rreq[rt] = n;
        // resources[rt] = ulm_new(resource_info, rreq[rt]);
        MD_add_constraint(rt, n);
    }

#ifdef _USE_TOPOLOGY_
    inline void
        add_constraint(resource_type rt, topology *t) {
        if (!((rt < R_TOTAL_RESOURCES) && (rt != R_NUM_RESOURCES))) {
            ulm_exit(("Aborting\n"));
        }
        if (!(t)) {
            ulm_exit(("Aborting\n"));
        }
        if (!(t->get_num_nodes() <= max_resources[rt])) {
            ulm_exit(("Aborting\n"));
        }
        /***
            if (rreq[rt] != 0) {
            // must free "old" info
            delete resources[rt];
            }
        ***/
        rreq[rt] = t->get_num_nodes();
        // resources[rt] = ulm_new(resource_info, rreq[rt]);
        MD_add_constraint(rt, t);
    }
#endif

    inline void
        add_constraint(resource_type rt, int n, constraint_info *c) {
        int i;
        if (!((rt < R_TOTAL_RESOURCES) && (rt != R_NUM_RESOURCES))) {
            ulm_exit(("Aborting\n"));
        }
        if (!(c)) {
            ulm_exit(("Aborting\n"));
        }
        for (i = 0; i < n; i++) {
            MD_add_constraint(rt, &c[i]);
        }
    }

    //
    // map requested resources
    //
    inline void
        map_resources() {
        MD_map_resources();
    }

private:
    // # of each resource requested
    int rreq[R_NUM_RESOURCES];
    // # of each resource already allocated
    int allocated_resources[R_NUM_RESOURCES];
    // # of each resource available
    int max_resources[R_NUM_RESOURCES];
    // info about the allocated resources
    resource_info *resources[R_NUM_RESOURCES];

    //
    //
    // MD_request_private.h MUST define:
    //
    //    void MD_init_resources()
    //         fill in max_resources[]
    //    void MD_destructor()
    //         clean up MD part
    //    void MD_add_constraint(resource_type, topology *) /*** not used ***/
    //    void MD_add_constraint(resource_type, constraint_info *)
    //         add contstraint to the resource mapping
    //    void MD_map_resources()
    //         fill in resources[]
    //

#include "os/IRIX/MD_request_private.h"
};

#endif /*** __REQUEST_H__ ***/
