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

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "client/ULMClient.h"

#include "os/IRIX/request.h"

void request::MD_map_resources()
{
    int res, res_p, num, mask;
    resources[R_CPU] = ulm_new(cpu_info, rreq[R_CPU]);
    if( rreq[R_HIPPI] ) {
	    resources[R_HIPPI] = ulm_new(hippi_info, rreq[R_HIPPI]);
    } else {
	     resources[R_HIPPI]=NULL;
    }

    if (rreq[R_MEMORY] == 0) {
	    rreq[R_MEMORY] = (rreq[R_CPU] + 1)/2;
    }

    if (rreq[R_MEMORY] > 0) {
        resources[R_MEMORY] = ulm_new(mem_info, rreq[R_MEMORY]);
    }
    else {
        resources[R_MEMORY] = NULL;
    }

    MD_init_map();

#ifdef _DO_SOMETHING_DUMB_
    //
    // do a "dumb" assignment
    //
    for (res_p = 0; res_p < R_NUM_RESOURCES; res_p++) {
	res = res_priority[res_p];
	for (num = max_resources[res]-1;
	     allocated_resources[res] < rreq[res] && num >= 0; num--) {
	    mask = all_resources[res][num].get_constraints()->get_mask();
	    if (((mask & C_MUST_NOT_USE) == C_NO_CONSTRAINTS) &&
		(!all_resources[res][num].is_allocated())) {
		MD_allocate_resource(res, num);
	    }
	}
	if (rreq[res] != allocated_resources[res]) {
	    cerr << "Warning: not enough resources available." << endl;
	    rreq[res] = allocated_resources[res];
	}
    }
#else
    //
    // try to cluster things by allocating remaining resources
    //  near ones that have already been allocated (i.e., constrained)
    //
    // the algorithm is greedy, but should be reasonable (knock on wood)
    //
    // if prioritized by node, it runs in time proportional to:
    //     number of types of resources (R_NUM_RESOURCES) *
    //     number of nodes (max_num_nodes) *
    //     max number of a particular resource (call it max_resource)
    //  which could be loosely interpreted as O(n^3).  fortunately,  it's
    //  actually more like O(n^2) since R_NUM_RESOURCES will always be
    //  some small constant (<20).  and if you really want to get picky,
    //  for the O2K:
    //     max_num_nodes <= 64
    //     max_resource <= 128
    //  so in reality, it's a constant time algorithm!
    //
    // if prioritized by module (which doesn't seem to naturally
    //  cluster as well), it runs in time proportional to:
    //     number of types of resources (R_NUM_RESOURCES) *
    //     number of modules (max_num_modules) *
    //     number of nodes per module (4) *
    //     max number of a particular resource (call it max_resource)
    //  which could be loosely interpreted as O(n^3), but is actually
    //  more like O(n^2).  and since for the O2K:
    //     max_num_modules <= 16
    //     max_resource <= 128
    //  again, it's a constant time algorithm!
    //
    // NOTE to self: the priority data structure should probably be
    //               an order list of true "sets" of nodes/modules
    //               where the sets elements have the same priority.
    //               the remaining resources would be allocated in a
    //               round robin fashion among the elements.  let's see
    //               how well/poorly this simpler approach does before
    //               implementing the set-based version.
    //
    int node, node_p, num_p;
    int *tmp = ulm_new(int, max_num_modules);
    for (res_p = 0; res_p < R_NUM_RESOURCES; res_p++) {
	// for each resource type by some (for now fixed) priority
	res = res_priority[res_p];
#ifdef _BLAH_
	cout << "allocating " << all_resources[res][0].get_name();
	cout << " resources" << endl;
#endif
#ifdef _USE_MODULE_PRIORITY_
	int mod, mod_p;
	for (mod_p = 0; mod_p < max_num_modules; mod_p++) {
	    // copy the priority list, since it will be modified
	    tmp[mod_p] = module_priority[mod_p];
	}
	for (mod_p = 0; mod_p < max_num_modules; mod_p++) {
	    // for each modules in increasing "constrained-ness"
	    mod = tmp[mod_p];
	    if (modules[mod].num != -1) {
#ifdef _BLAH_
		if (modules[mod].get_total_allocated() != 0) {
		    cout << "   considering module " << mod;
		    cout << " priority " << modules[mod].get_total_allocated() << endl;
		}
#endif
		for (node_p = 0; node_p < modules[mod].num_nodes; node_p ++) {
		    // for each node in increasing "constrained-ness"
		    //  node priorites within a module not actually implemented
		    //  same as considering all the nodes
		    if ((node = modules[mod].get_priority_node(node_p)) != -1) {
			for (num_p = 0;
			     ((allocated_resources[res] < rreq[res]) &&
			      (num_p < nodes[node].num_resource[res])) ;
			     num_p++) {
			    // for each resource available
			    if ((num = nodes[node].get_resource_num(res, num_p)) != -1) {
				mask = all_resources[res][num].get_constraints()->get_mask();
				if (((mask & C_MUST_NOT_USE) == C_NO_CONSTRAINTS) &&
				    (!all_resources[res][num].is_allocated())) {
				    // cout << "module " << mod << " node " << node << " res " << res << " num " << num << endl;
				    MD_allocate_resource(res, num);
				}
			    }
			}
		    }
		}
	    }
	}
#else
	for (node_p = 0; node_p < max_num_nodes; node_p++) {
	    // copy the priority list, since it will be modified
	    tmp[node_p] = node_priority[node_p];
	}
	for (node_p = 0; node_p < max_num_nodes; node_p++) {
	    // for each node in increasing "constrained-ness"
	    node = tmp[node_p];
#ifdef _BLAH_
	    if (nodes[node].get_total_allocated() != 0) {
		cout << "   considering node " << node;
		cout << " priority " << nodes[node].get_total_allocated() << endl;
	    }
#endif // _BLAH_
	    for (num_p = 0;
		 ((allocated_resources[res] < rreq[res]) &&
		  (num_p < nodes[node].num_resource[res])) ;
		 num_p++) {
		// for each resource available
		if ((num = nodes[node].get_resource_num(res, num_p)) != -1) {
		    mask = all_resources[res][num].get_constraints()->get_mask();
		    if (((mask & C_MUST_NOT_USE) == C_NO_CONSTRAINTS) &&
			(!all_resources[res][num].is_allocated())) {
			// cout << "module " << mod << " node " << node << " res " << res << " num " << num << endl;
			MD_allocate_resource(res, num);
		    }
		}
	    }
	}
#endif
    }
    ulm_delete(tmp);
    for (res = 0; res < R_NUM_RESOURCES; res++) {
	if (rreq[res] > allocated_resources[res]) {
	    cerr << "Warning: not enough " << all_resources[res][0].get_name();
	    cerr << " resources available." << endl;
	    rreq[res] = allocated_resources[res];
	}
    }
#endif
}

void request::MD_init_resource_info()
{
    int retval;
    FILE *f;
    char path[256];
    // module info
    int mod_num, max_mod_num = -1;
    // node info
    int node_num, max_node_num = -1;
    // cpu info
    int cpu_num, max_cpu_num = -1;
    int mod, node;
    char slot;
    cpu_info *cpus;
    // hippi device info
    int hippi_num, max_hippi_num = -1;
    int ioslot;
    hippi_info *hippi_devs;
    // memory info
    mem_info *memories;

    int guess_modules = 100;
    int guess_nodes = 64;
    int guess_cpus = 128;
    int guess_hippi_devs = 13;
    int guess_memories = guess_nodes;

    //
    // find max available modules
    //
    max_num_modules = guess_modules;
    modules = ulm_new(module_info, guess_modules);
#ifdef _USE_MODULE_PRIORITY_
    module_priority = ulm_new(int, guess_modules);
#endif
    if ((f = popen("/usr/bin/ls -FS1 /hw/module", "r")) != NULL) {
	while (fscanf(f, "%d -> %s ", &mod_num, path) != EOF) {
	    if (!(strlen(path) < 256)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_module_num(mod_num))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (mod_num > max_mod_num) {
		max_mod_num = mod_num;
	    }
	    modules[mod_num].num = mod_num;
	}
    }
    pclose(f);
    max_num_modules = max_mod_num+1;
#ifdef _USE_MODULE_PRIORITY_
    for (mod_num = 0; mod_num < max_num_modules; mod_num++) {
	// prioritize in reverse order as the default
	module_priority[mod_num] = max_num_modules-(mod_num+1);
    }
#endif

    //
    // find max available nodes (and memories)
    //
    max_num_nodes = guess_nodes;
    nodes = ulm_new(node_info, guess_nodes);
    max_resources[R_MEMORY] = guess_memories;
#ifdef _USE_MODULE_PRIORITY_
#else
    node_priority = ulm_new(int, guess_nodes);
#endif
    memories = ulm_new(mem_info, guess_memories);
    if ((f = popen("/usr/bin/ls -FS1 /hw/nodenum", "r")) != NULL) {
	while (fscanf(f, "%d -> %s ", &node_num, path) != EOF) {
	    if (!(strlen(path) < 256)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_memory_num(node_num))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_node_num(node_num))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (node_num > max_node_num) {
		max_node_num = node_num;
	    }

	    retval = sscanf(path, "/hw/module/%d/slot/n%d/node", &mod, &node);
	    if (!(retval == 2)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_module_num(mod))) {
		ulm_exit(("Aborting\n"));
	    }

	    // set up node info
	    nodes[node_num].num = node_num;
	    nodes[node_num].module = mod;
	    nodes[node_num].node_num = node;
	    modules[mod].set_node(node, node_num);

	    // set up memory info
	    memories[node_num].set_num(node_num);
	    memories[node_num].set_path(path);
	    memories[node_num].set_module(mod);
	    memories[node_num].set_node(node);
	    nodes[node_num].add_memory(node_num);
	}
    }
    pclose(f);
    max_num_nodes = max_node_num+1;
    max_resources[R_MEMORY] = max_node_num+1;
    all_resources[R_MEMORY] = memories;
#ifdef _USE_MODULE_PRIORITY_
#else
    for (node_num = 0; node_num < max_num_nodes; node_num++) {
	// prioritize in reverse order as the default
	node_priority[node_num] = max_num_nodes-(node_num+1);
    }
#endif

    //
    // find max available CPUs
    //
    max_resources[R_CPU] = guess_cpus;
    cpus = ulm_new(cpu_info, guess_cpus);
    if ((f = popen("/usr/bin/ls -FS1 /hw/cpunum", "r")) != NULL) {
	while (fscanf(f, "%d -> %s ", &cpu_num, path) != EOF) {
	    if (!(strlen(path) < 256)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_cpu_num(cpu_num))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (cpu_num > max_cpu_num) {
		max_cpu_num = cpu_num;
	    }
	    cpus[cpu_num].set_num(cpu_num);
	    cpus[cpu_num].set_path(path);

	    /*** WARNING *** WARNING *** WARNING ***/
	    /*** hardware path for cpu numbers is different on n01 and n02 ***/
	    /*** THIS IS NOT A GENERAL FIX *** THERE MAY BE NONE ***/
	    /***    n01 -> "/hw/module/%d/slot/n%d/node/cpubus/0/%c" ***/
	    /***    n02 -> "/hw/module/%d/slot/n%d/node/cpubus/0/%c" ***/
	    /***    n02 -> "/hw/module/%d/slot/n%d/node/cpu/%c" ***/
	    retval = sscanf(path, "/hw/module/%d/slot/n%d/node/cpubus/0/%c",
			    &mod, &node, &slot);
	    if (retval != 3) { /*** try the other path style ***/
		retval = sscanf(path, "/hw/module/%d/slot/n%d/node/cpu/%c",
				&mod, &node, &slot);
	    }

	    if (!(retval == 3)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_module_num(mod))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_node(node))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_cpu_slot(slot))) {
		ulm_exit(("Aborting\n"));
	    }

	    cpus[cpu_num].set_module(mod);
	    cpus[cpu_num].set_node(node);
	    cpus[cpu_num].set_slot(slot);
	    nodes[modules[mod].get_node(node)].set_cpu((slot=='a'?0:1), cpu_num);
	}
    }
    pclose(f);
    max_resources[R_CPU] = max_cpu_num+1;
    all_resources[R_CPU] = cpus;

    //
    // find max available HIPPI devices
    //
    max_resources[R_HIPPI] = guess_hippi_devs;
    hippi_devs = ulm_new(hippi_info, guess_hippi_devs);
    if ((f = popen("/usr/bin/ls -FS1 /hw/hippi", "r")) != NULL) {
	while (fscanf(f, "%d -> %s ", &hippi_num, path) != EOF) {
	    if (!(strlen(path) < 256)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_hippi_num(hippi_num))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (hippi_num > max_hippi_num) {
		max_hippi_num = hippi_num;
	    }
	    hippi_devs[hippi_num].set_num(hippi_num);
	    hippi_devs[hippi_num].set_path(path);

	    retval = sscanf(path, "/hw/module/%d/slot/io%d/", &mod, &ioslot);
	    if (!(retval == 2)) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_module_num(mod))) {
		ulm_exit(("Aborting\n"));
	    }
	    if (!(MD_is_valid_xio_slot(ioslot))) {
		ulm_exit(("Aborting\n"));
	    }

	    hippi_devs[hippi_num].set_module(mod);
	    hippi_devs[hippi_num].set_ioslot(ioslot);
	    hippi_devs[hippi_num].set_max_jobs(MD_hippi_max_jobs(hippi_num));

	    nodes[modules[mod].get_node(hippi_devs[hippi_num].get_node_num(0))].add_hippi(hippi_num);
	    nodes[modules[mod].get_node(hippi_devs[hippi_num].get_node_num(1))].add_hippi(hippi_num);
	}
    }
    pclose(f);
    for (hippi_num = 0; hippi_num < max_hippi_num+1; hippi_num++) {
	if (hippi_devs[hippi_num].get_max_jobs() < 1) {
	    // can't use this board
	    hippi_devs[hippi_num].get_constraints()->or_mask(C_MUST_NOT_USE);
	}
    }
    max_resources[R_HIPPI] = max_hippi_num+1;
    all_resources[R_HIPPI] = hippi_devs;

    // order in which resource assignment should be processed -- priority
    //  is given to the type that is most constrained (i.e., least available)
    res_priority[0] = R_HIPPI;
    res_priority[1] = R_MEMORY;
    res_priority[2] = R_CPU;
}

void request::MD_add_constraint(resource_type rt, constraint_info *c)
{
    int i, num, mod_num, node, node_num, h, hippi_num;
    int cpu_a, cpu_b;
    constraint_info my_c, tc;

    if (!(MD_is_valid_resource(rt, c->get_num()))) {
	ulm_exit(("Aborting\n"));
    }
    num = c->get_num();

    my_c = *c;
#ifdef _BLAH_
    cout << "mask IN " << all_resources[rt][num].get_constraints()->get_mask() << endl;
#endif
    for (i = C_MUST_USE; i < C_UNUSED; i <<= 1) {
	switch (i&my_c.get_mask()) {
	case C_MUST_USE:      // must use this resource
	    all_resources[rt][num].get_constraints()->and_mask(~(C_MUST_NOT_USE));
	    break;
	case C_MUST_NOT_USE:  // must NOT use this resource
	    all_resources[rt][num].get_constraints()->and_mask(~(C_MUST_USE));
	    break;
	case C_NO_CPU:
	    tc.set_type(R_CPU);
	    tc.reset_mask(C_MUST_NOT_USE);
	    mod_num = all_resources[rt][num].get_module();
	    switch (rt) {
	    default:
		node = all_resources[rt][num].get_node();
		if (!(MD_is_valid_node(node))) {
		    ulm_exit(("Aborting\n"));
		}
		node_num = modules[mod_num].get_node(node);
		// connected to 2 cpus
		tc.set_num(nodes[node_num].getCpu(0));
		MD_add_constraint(R_CPU, &tc);
		tc.set_num(nodes[node_num].getCpu(1));
		MD_add_constraint(R_CPU, &tc);
		break;
	    case R_HIPPI:
		hippi_info *hi;
		int start, end;
		hi = static_cast<hippi_info *>(&(all_resources[rt][num]));
		start = hi->get_node_num(0); end = hi->get_node_num(1);
		// connected to 2 nodes
		for (node = start; node <= end; node += 2) {
		    node_num = modules[mod_num].get_node(node);
		    // connected to 2 cpus
		    tc.set_num(nodes[node_num].getCpu(0));
		    MD_add_constraint(R_CPU, &tc);
		    tc.set_num(nodes[node_num].getCpu(1));
		    MD_add_constraint(R_CPU, &tc);
		}
		break;
	    case R_CPU:
		ulm_exit(("Aborting\n"));
		break;
	    }
	    break;
	case C_NO_HIPPI:
	    if (!(rt != R_HIPPI)) {
		ulm_exit(("Aborting\n"));
	    }
	    tc.set_type(R_HIPPI);
	    tc.reset_mask(C_MUST_NOT_USE);
	    mod_num = all_resources[rt][num].get_module();
	    node = all_resources[rt][num].get_node();
	    if (!(MD_is_valid_node(node))) {
		ulm_exit(("Aborting\n"));
	    }
	    if ((node == 1) || (node == 3)) {
		node_num = modules[mod_num].get_node(node);
		for (h = 0; h < nodes[node_num].num_hips; h++) {
		    hippi_num = nodes[node_num].get_hippi(h);
		    if (hippi_num != -1) {
			tc.set_num(hippi_num);
			MD_add_constraint(R_HIPPI, &tc);
		    }
		}
	    } else {
		// do nothing
	    }
	    break;
	case C_NO_MEMORY:
	    tc.set_type(R_MEMORY);
	    tc.reset_mask(C_MUST_NOT_USE);
	    mod_num = all_resources[rt][num].get_module();
	    switch (rt) {
	    default:
		node = all_resources[rt][num].get_node();
		if (!(MD_is_valid_node(node))) {
		    ulm_exit(("Aborting\n"));
		}
		tc.set_num(modules[mod_num].get_node(node));
		MD_add_constraint(R_MEMORY, &tc);
		break;
	    case R_HIPPI:
		hippi_info *hi;
		// connected to 2 nodes
		hi = static_cast<hippi_info *>(&(all_resources[rt][num]));
		tc.set_num(modules[mod_num].get_node(hi->get_node_num(0)));
		MD_add_constraint(R_MEMORY, &tc);
		tc.set_num(modules[mod_num].get_node(hi->get_node_num(1)));
		MD_add_constraint(R_MEMORY, &tc);
		break;
	    case R_MEMORY:
		ulm_exit(("Aborting\n"));
		break;
	    }
	    break;
	case C_CPU_PER_NODE:
	    // make this as unconstrained as possible:
	    //  - traverse through the list of nodes and find the cpu(s)
	    //    associated with each node.
	    //  - if 'a' exists (and 'b' is not constrained as "must use")
	    //    then use 'a' and constrain 'b' to C_MUST_NOT_USE,
	    //  - else if 'b' exists then use 'b'
	    //  - else do nothing.
	    //  *** this doesn't work ask expected, so it is commented out ***
	    //  - truncate the path of the cpu that is being used to the path
	    //    of the node.
	    // the cpus are now specified by node only and something else -- for
	    //  example, mldset_create() -- will decide which of the two cpus
	    //  to use.
	    //
	    // i wonder if this will work as expected..
	    //
	    for (node = 0; node < max_num_nodes; node++) {
		cpu_a = nodes[node].getCpu(0);
		cpu_b = nodes[node].getCpu(1);
		if ((cpu_a != -1) &&
		    ((cpu_b != -1) &&
		     ((all_resources[R_CPU][cpu_b].get_constraints()->get_mask() &
		       C_MUST_USE) != C_MUST_USE))) {
		    // use 'a'
		    char *path = ulm_new(char,
                                         strlen(all_resources[R_CPU][cpu_a].get_path())+1);
		    strcpy(path, all_resources[R_CPU][cpu_a].get_path());
#ifdef _IT_DONT_WORK_
		    // someone should punish me for doing this..
#ifdef _OLD_PATH_STYLE_
		    char *tail = strstr(path, "/cpu/a");
#else
		    char *tail = strstr(path, "/cpubus/0/a");
#endif
		    *tail = '\0';
#endif
		    all_resources[R_CPU][cpu_a].set_path(path);
		    ulm_delete(path);
		    if (cpu_b != -1) {
			// don't use 'b'
			tc.set_type(R_CPU);
			tc.reset_mask(C_MUST_NOT_USE);
			tc.set_num(cpu_b);
			MD_add_constraint(R_CPU, &tc);
		    }
		} else if (cpu_b != -1) {
		    // use 'b'
		    char *path = ulm_new(char, strlen(all_resources[R_CPU][cpu_b].get_path())+1);
		    strcpy(path, all_resources[R_CPU][cpu_b].get_path());
#ifdef _IT_DONT_WORK_
		    // again, someone should punish me for doing this..
#ifdef _OLD_PATH_STYLE_
		    char *tail = strstr(path, "/cpu/b");
#else
		    char *tail = strstr(path, "/cpubus/0/b");
#endif
		    *tail = '\0';
#endif
		    all_resources[R_CPU][cpu_b].set_path(path);
		    ulm_delete(path);
		    if (cpu_a != -1) {
			// don't use 'a'
			tc.set_type(R_CPU);
			tc.reset_mask(C_MUST_NOT_USE);
			tc.set_num(cpu_a);
			MD_add_constraint(R_CPU, &tc);
		    }
		} else {
		    // do nothing
		}
	    }
	    break;
	case C_NO_CONSTRAINTS:
	default:
	    break;
	}
    }
    my_c.or_mask(all_resources[rt][num].get_constraints()->get_mask());
    all_resources[rt][num].set_constraints(&my_c);

#ifdef _BLAH_
    cout << "mask OUT " << all_resources[rt][num].get_constraints()->get_mask() << endl;
#endif
}


void request::MD_allocate_resource(int res, int num)
{
    // get info about allocated resource
    // resources[res][allocated_resources[res]++] = all_resources[res][num];
    resources[res][allocated_resources[res]++].copy_me(all_resources[res][num]);

    // flag as allocated
    all_resources[res][num].allocate();

    // "re-sort" the priority list
    // NOTE: implementation should be rethought if length of the priority
    //       list increases significantly.
    //    - O(N) insertion
    //    - O(N) deletion
    //    - O(1) sort
    //
    int n, priority, head, tail, cursor;
    //
    // you gotta just love macro programming!
#ifdef _USE_MODULE_PRIORITY_
#define _PRIORITY_TYPE_ modules
#define _PRIORITY_LIST_ module_priority
#define _PRIORITY_LIST_LEN_ max_num_modules
    n = all_resources[res][num].get_module();
#else
#define _PRIORITY_TYPE_ nodes
#define _PRIORITY_LIST_ node_priority
#define _PRIORITY_LIST_LEN_ max_num_nodes
    int mod = all_resources[res][num].get_module();
    int res_nodes[2], num_res_nodes, res_node;
    n = all_resources[res][num].get_node();
    if (n == -1) {
	// connected to 2 nodes
	hippi_info *hi = static_cast<hippi_info *>(&(all_resources[res][num]));
	num_res_nodes = 2;
	res_nodes[0] = modules[mod].get_node(hi->get_node_num(0));
	res_nodes[1] = modules[mod].get_node(hi->get_node_num(1));
    } else {
	num_res_nodes = 1;
	res_nodes[0] = modules[mod].get_node(n);
    }
    for (res_node = 0; res_node < num_res_nodes; res_node++) {
	n = res_nodes[res_node];
#endif
	_PRIORITY_TYPE_[n].allocate_resource(res);
	priority = _PRIORITY_TYPE_[n].get_total_allocated();
	for (head = 0; head < _PRIORITY_LIST_LEN_; head++) {
	    if (_PRIORITY_TYPE_[_PRIORITY_LIST_[head]].get_total_allocated() <= priority) {
		break; // found our head
	    }
	}
	if (!(head < _PRIORITY_LIST_LEN_)) {
	    ulm_exit(("Aborting\n"));
	}
	for (tail = head; tail < _PRIORITY_LIST_LEN_; tail++) {
	    if (_PRIORITY_LIST_[tail] == n) {
		break; // found our tail
	    }
	}
	if (!(tail < _PRIORITY_LIST_LEN_)) {
	    ulm_exit(("Aborting\n"));
	}
	for (cursor = tail; cursor > head; cursor--) {
	    // move everyone down
	    _PRIORITY_LIST_[cursor] = _PRIORITY_LIST_[cursor-1];
	}
	_PRIORITY_LIST_[head] = n;
#ifdef _USE_MODULE_PRIORITY_
#else
    }
#endif
#undef _PRIORITY_TYPE_
#undef _PRIORITY_LIST_
#undef _PRIORITY_LIST_LEN
}

bool request::MD_is_valid_resource(resource_type rt, int r)
{
    if (!(rt < R_NUM_RESOURCES)) {
	ulm_exit(("Aborting\n"));
    }
    if ((r >= 0) && (r < max_resources[rt])) {
	return(true);
    }
    else {
	return(false);
    }
}
