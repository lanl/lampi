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

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "os/IRIX/request.h"

int resource_info::get_other(int n)
{
    if (!(n < num_other)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (other[n]);
}

void resource_info::set_other(int n, int value)
{
    if (!(n < num_other)) {
        ulm_exit((-1, "Aborting \n"));
    }
    other[n] = value;
}

void io_info::set_ioslot(int s)
{
    // assign the node number
    if (s > 0) {
        if (!(s <= 12)) {
            ulm_exit((-1, "Aborting \n"));
        }
        if (s <= 6) {
            set_other(0, 1);
            set_other(1, 3);
        } else {
            set_other(0, 2);
            set_other(1, 4);
        }
    } else {
        set_other(0, -1);
        set_other(1, -1);
    }
    set_other(2, s);
}

int io_info::get_node_num(int s)
{
    if (!((s >= 0) && (s < 2))) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (get_other(s));
}

int router_info::add_node(int i)
{
    if (!(node_cursor + 1 < num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    node[++node_cursor] = i;
    return (node_cursor);
}
int router_info::set_node(int s, int i)
{
    if (!(s < num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (node[s] = i);
}
int router_info::get_node(int s)
{
    if (!(s < num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (node[s]);
}

int router_info::add_router(int i)
{
    if (!(router_cursor + 1 < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    router[++router_cursor] = i;
    return (router_cursor);
}
int router_info::set_router(int s, int i)
{
    if (!(s < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (router[s] = i);
}
int router_info::get_router(int s)
{
    if (!(s < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (router[s]);
}

int node_info::add_cpu(int i)
{
    if (!(cpu_cursor + 1 < num_cpus)) {
        ulm_exit((-1, "Aborting \n"));
    }
    cpu[++cpu_cursor] = i;
    resource[R_CPU][cpu_cursor] = i;
    return (cpu_cursor);
}
int node_info::set_cpu(int s, int i)
{
    if (!(s < num_cpus)) {
        ulm_exit((-1, "Aborting \n"));
    }
    resource[R_CPU][s] = i;
    return (cpu[s] = i);
}
int node_info::getCpu(int s)
{
    if (!(s < num_cpus)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (cpu[s]);
}


int node_info::add_hippi(int i)
{
    if (!(hippi_cursor + 1 < num_hips)) {
        ulm_exit((-1, "Aborting \n"));
    }
    hippi[++hippi_cursor] = i;
    resource[R_HIPPI][hippi_cursor] = i;
    return (hippi_cursor);
}
int node_info::set_hippi(int s, int i)
{
    if (!(s < num_hips)) {
        ulm_exit((-1, "Aborting \n"));
    }
    resource[R_HIPPI][s] = i;
    return (hippi[s] = i);
}
int node_info::get_hippi(int s)
{
    if (!(s < num_hips)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (hippi[s]);
}

int node_info::add_memory(int i)
{
    if (!(memory_cursor + 1 < num_mems)) {
        ulm_exit((-1, "Aborting \n"));
    }
    memory[++memory_cursor] = i;
    resource[R_MEMORY][memory_cursor] = i;
    return (memory_cursor);
}

int node_info::set_memory(int s, int i)
{
    if (!(s < num_mems)) {
        ulm_exit((-1, "Aborting \n"));
    }
    resource[R_MEMORY][s] = i;
    return (memory[s] = i);
}

int node_info::get_memory(int s)
{
    if (!(s < num_mems)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (memory[s]);
}

int node_info::add_router(int i)
{
    if (!(router_cursor + 1 < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    router[++router_cursor] = i;
    return (router_cursor);
}

int node_info::set_router(int s, int i)
{
    if (!(s < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (router[s] = i);
}

int node_info::get_router(int s)
{
    if (!(s < num_routers)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (router[s]);
}

int module_info::add_node(int i)
{
    if (!(node_cursor < num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    node[++node_cursor] = i;
    node_priority[node_cursor] = i;
    return (node_cursor);
}

int module_info::set_node(int s, int i)
{
    if (!(s <= num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    node_priority[s - 1] = i;
    return (node[s - 1] = i);
}

int module_info::get_node(int s)
{
    if (!(s <= num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (node[s - 1]);
}

int module_info::get_priority_node(int s)
{
    if (!(s <= num_nodes)) {
        ulm_exit((-1, "Aborting \n"));
    }
    return (node_priority[s - 1]);
}
