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
#ifdef _USE_MODULE_PRIORITY_
int *module_priority; // priority by number of constrained resources
#else
int *node_priority;   // priority by number of constrained resources
#endif
int max_num_nodes;    // total number of nodes

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
void MD_init_resource_info() {return;}

//
// REQUIRED: MD_destructor()
//
inline void
MD_destructor() { }


//
// REQUIRED: MD_add_constraint()
//
// add constraints to the resource mapping
//
inline void
MD_add_constraint(resource_type rt, int n) {
    // nothing special needed here
}
//
//WARNING: this method is recursive and passes stack variables
//
void MD_add_constraint(resource_type rt, constraint_info *c) {return;}

//
// REQUIRED: MD_add_constraints_from_file()
//
void MD_add_contraints_from_file(FILE *f) {return;}

//
// REQUIRED: MD_map_resources()
//
// assign resources according to contraints
//
void MD_map_resources() {return;}

//
// MD_init_map()
//
// "private" machine dependent routine to initailize the mapping
//   according to the specified constraints
//
// no topology info used yet
//
inline void
MD_init_map() {return;}

//
// MD_allocate_resource()
//
// "private" method that does what needs to be done to internal
//   data structures when a resource is allocated
//
void MD_allocate_resource(int res, int num){return;}


//
// print out the topology for debugging purposes
//
public:
inline void
MD_print() {return;}

//
// some utility functions
//
private:
bool MD_is_valid_resource(resource_type rt, int r) {return false;}

inline bool
MD_is_valid_module_num(int m) {return false;}

#endif /*** __MD_REQUEST_PRIVATE_H__ ***/
