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



#ifndef __MD_REQUEST_PRIVATE_H__
#define __MD_REQUEST_PRIVATE_H__

private:
// probably should be a hash table of some sort
resource_info *all_resources[R_NUM_RESOURCES];
int res_priority[R_NUM_RESOURCES];  // priority by resource type
int max_num_modules;  // total number of modules
module_info *modules; // module data
#ifdef _USE_MODULE_PRIORITY_
int *module_priority; // priority by number of constrained resources
#else
int *node_priority;   // priority by number of constrained resources
#endif
int max_num_nodes;    // total number of nodes
node_info *nodes;     // node data

//
// REQUIRED: MD_init_resource_info()
//
// find max number available for each resource and initialize
//  any other MD info about the resources we want
//      - mapping of resource numbers (name) to hw path
//
//  ASSUMPTIONS:
//      - ONE BOX only
//      - CPU and HIPPI device numbering is zero-based and contiguous
//      - max number of CPUs is 128 (though it may be less)
//      - max number of HIPPI devices is 5 (though it may be less)
//
void MD_init_resource_info() ;

//
// REQUIRED: MD_destructor()
//
inline void
MD_destructor() {
    ulm_delete(modules);
#ifdef _USE_MODULE_PRIORITY_
    ulm_delete(module_priority);
#else
    ulm_delete(node_priority);
#endif
    ulm_delete(nodes);

    ulm_delete(all_resources[R_CPU]);
    ulm_delete(all_resources[R_HIPPI]);
    ulm_delete(all_resources[R_MEMORY]);

    ulm_delete(resources[R_CPU]);
    if( resources[R_HIPPI] != NULL )
	    ulm_delete(resources[R_HIPPI]);
    ulm_delete(resources[R_MEMORY]);
}


//
// REQUIRED: MD_add_constraint()
//
// add constraints to the resource mapping
//
inline void
MD_add_constraint(resource_type rt, int n) {
    // nothing special needed here
}
#ifdef _USE_TOPOLOGY_
inline void
MD_add_constraint(resource_type rt, topology *t) {
    // no topology info used yet
}
#endif
//
// yuck!
//
// WARNING: this method is recursive and passes stack variables
//
void MD_add_constraint(resource_type rt, constraint_info *c);

//
// REQUIRED: MD_map_resources()
//
// assign resources according to contraints
//
void MD_map_resources() ;

//
// MD_init_map()
//
// "private" machine dependent routine to initailize the mapping
//   according to the specified constraints
//
// no topology info used yet
//
inline void
MD_init_map() {
    int res, num;
    for (res = 0; res < R_NUM_RESOURCES; res++) {
        for (num = max_resources[res]-1; num >=0; num--) {
            if ((all_resources[res][num].get_constraints()->get_mask() & C_MUST_USE)
                == C_MUST_USE) {
                if (allocated_resources[res] < rreq[res]) {
                    MD_allocate_resource(res, num);
                } else {
                    cerr << "Warning: number of \"must use\" ";
                    cerr << all_resources[res][num].get_name();
                    cerr << " resources exceeds number requested..\n";
                    cerr << "         (ignoring request for ";
                    cerr << all_resources[res][num].get_name();
                    cerr << " " << num << ")" << endl;
                }
            }
        }
    }
}

//
// MD_allocate_resource()
//
// "private" method that does what needs to be done to internal
//   data structures when a resource is allocated
//
void MD_allocate_resource(int res, int num);


//
// print out the topology for debugging purposes
//
public:
inline void
MD_print() {
    int m, n;
    for (m = 0; m < max_num_modules; m++) {
        if (modules[m].num != -1) {
            cout << "##############" << endl;
            modules[m].print();
            for (n = 1; n <= 4; n++) {
                if (modules[m].get_node(n) != -1) {
                    cout << "### ";
                    nodes[modules[m].get_node(n)].print();
                }
            }
        }
    }
}

//
// some utility functions
//
private:
bool MD_is_valid_resource(resource_type rt, int r) ;

inline bool
MD_is_valid_module_num(int m) {
    if ((m >= 0) && (m < max_num_modules)) {
        return(true);
    } else {
        return(false);
    }
}

inline bool
MD_is_valid_node(int n) {
    if ((n >= 1) && (n <= 4)) {
        return(true);
    } else {
        return(false);
    }
}

inline bool
MD_is_valid_node_num(int n) {
    if ((n >= 0) && (n < max_num_nodes)) {
        return(true);
    } else {
        return(false);
    }
}

inline bool
MD_is_valid_cpu_num(int c) {
    return(MD_is_valid_resource(R_CPU, c));
}

inline bool
MD_is_valid_cpu_slot(char c) {
    if ((c == 'a') || (c == 'b')) {
        return(true);
    } else {
        return(false);
    }
}

inline bool
MD_is_valid_hippi_num(int h) {
    return(MD_is_valid_resource(R_HIPPI, h));
}

inline bool
MD_is_valid_memory_num(int m) {
    return(MD_is_valid_resource(R_MEMORY, m));
}

inline bool
MD_is_valid_xio_slot(int slot) {
    if ((slot >= 1) && (slot <= 6)) {
        // connected to nodes 1 & 3
        return(true);
    } else if ((slot >= 7) && (slot <= 12)) {
        // connected to nodes 2 & 4
        return(true);
    } else {
        return(false);
    }
}

inline int
MD_hippi_max_jobs(int board) {
    char ctl[100];
    int fd;
    struct hip_bp_config h_info;

#ifdef _PURIFY_
    bzero((void *) &h_info, sizeof(struct hip_bp_config));
#endif

    sprintf(ctl, "/hw/hippi/%d/bypass/ctl", board);
#ifdef _TEST_THIS_STUPID_HYPOTHESIS_
    if (board == 0) {
        if ((fd = open(ctl, O_RDONLY)) < 0) {
            cerr << "here!" << endl;
            close(fd);
            return(0);
        }
        cerr << fd << " THERE!" << endl;
        close(fd);
    }
#endif
    if ((fd = open(ctl, O_RDONLY)) < 0) {
        return(0);
    }
    // cerr << "fd=" << fd << " getting info about " << ctl << " (board " << board << ")" << endl;

    if (ioctl(fd, HIPIOC_GET_BPCFG, &h_info) < 0) {
        // cerr << "hi!! ioctl failed: board " << board << " (" << errno << ")..  let's try again." << endl;
        if (ioctl(fd, HIPIOC_GET_BPCFG, &h_info) < 0) {
            // cerr << "     ioctl failed: board " << board << " (" << errno << ")..  i give up!" << endl;
            close(fd);
            return(0);
        }
        // cerr << "     YEAH!!!  it worked!" << endl;
    }
    close(fd);
    // cerr << "got info for " << ctl << " (board " << board << ")" << endl;
    return(h_info.max_jobs);
}

#endif /*** __MD_REQUEST_PRIVATE_H__ ***/
