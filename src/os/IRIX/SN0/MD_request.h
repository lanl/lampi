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

#ifndef __MD_REQUEST_H__
#define __MD_REQUEST_H__

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <stropts.h>
#include <sys/hippi.h>

#include "internal/new.h"

//
// REQUIRED: resource_type
//
typedef enum _rt {
    // allocatable resources
    R_CPU=0,
    R_HIPPI,
    R_MEMORY,
    R_NUM_RESOURCES,
    // other resources
    R_MODULE,
    R_NODE,
    R_ROUTER,
    R_METAROUTER,
    R_TOTAL_RESOURCES
} resource_type;

typedef enum _ct {
    C_NO_CONSTRAINTS = 0x0000,
    C_MUST_USE       = 0x0001,
    C_MUST_NOT_USE   = 0x0002,
    C_NO_CPU         = 0x0004,
    C_NO_HIPPI       = 0x0008,
    C_NO_MEMORY      = 0x0010,
    C_CPU_PER_NODE   = 0x0020,
    C_UNUSED         = 0x0040
} constraint_type;

//
// REQUIRED: constraint_info
//
class
constraint_info {
public:
    constraint_info(resource_type rt=R_NUM_RESOURCES,
                    int n=-1,
                    constraint_type m=C_NO_CONSTRAINTS) {
        type = rt;
        num = n;
        mask = m;
    }
    constraint_info(const constraint_info& _c) {
        type = _c.type;
        num = _c.type;
        mask = _c.mask;
    }
    ~constraint_info() {
    }

    inline void set_type(resource_type rt) { type = rt; }
    inline resource_type get_type()        { return(type); }

    inline void set_num(int n)             { num = n; }
    inline int get_num()                   { return(num); }

    inline void reset_mask()               { mask = C_NO_CONSTRAINTS; }
    inline void reset_mask(int m)          { mask = m; }
    inline int get_mask()                  { return(mask); }
    inline int and_mask(int m)             { return(mask &= m); }
    inline int or_mask(int m)              { return(mask |= m); }
    inline int xor_mask(int m)             { return(mask=(mask|m)&~(mask&m)); }

private:
    resource_type type; // type of resource being constrained
    int num;            // resource number
    int mask;           // bit mask describing the constraints
};

//
// REQUIRED: resource_type
//
// user allocatable resources
//
class
resource_info {
public:
    resource_info() {
        deallocate();
        set_type(R_NUM_RESOURCES);
        set_num(-1);
        ci = new constraint_info;
        path = NULL;
        set_module(-1);
        set_node(-1);
        set_name("");
    }
    resource_info(resource_info& ri) {
        deallocate();
        ci = new constraint_info;
        path = NULL;
        copy_me(ri);
    }
    ~resource_info() {
        if (path != NULL) {
            ulm_delete(path);
        }
        delete ci;
    }

    inline void
        copy_me(resource_info& ri) {
        set_type(ri.get_type());
        set_num(ri.get_num());
        set_constraints(ri.get_constraints());
        set_path(ri.get_path());
        set_module(ri.get_module());
        set_node(ri.get_node());
        set_name(ri.get_name());
    }

    inline resource_type get_type()           { return(type); }
    inline int get_num()                      { return(num); }
    inline constraint_info *get_constraints() { return(ci); }
    inline char *get_path()                   { return(path); }
    inline int get_module()                   { return(module); }
    inline int get_node()                     { return(node); }
    inline char *get_name()                   { return(name); }
    int get_other(int n);

    inline void set_type(resource_type _type) { type = _type; }
    inline void set_num(int _num)             { num = _num; }
    inline void set_constraints(constraint_info *_ci) {
        ci->set_type(_ci->get_type());
        ci->set_num(_ci->get_num());
        ci->reset_mask(_ci->get_mask());
    }
    inline void set_path(const char *_path) {
        if (_path == NULL) {
            path = NULL;
        }
        else {
            if (path != NULL) {
                ulm_delete(path);
            }
            path = ulm_new(char, strlen(_path)+1);
            strcpy(path, _path);
        }
    }
    inline void set_module(int _module)     { module = _module; }
    inline void set_node(int _node)         { node = _node; }
    inline void set_name(char *_name)       { name = _name; }
    void set_other(int n, int value);

    inline bool is_allocated() { return(allocated); }
    inline void allocate()     { allocated = true; }
    inline void deallocate()   { allocated = false; }

    virtual inline void print() {}

protected:
    resource_type type;
    int num;
    constraint_info *ci;
    char *path;
    int module;
    int node;
    char *name;
    static const int num_other = 10;
    int other[num_other];
    bool allocated;
};

class
cpu_info: public resource_info {
public:
    cpu_info(int _num=-1, const char *_path=NULL,
             int _module=-1, int _node=-1, char _slot='x') {
        deallocate();
        set_type(R_CPU);
        set_num(_num);
        // ci = new constraint_info;
        set_path(_path);
        set_module(_module);
        set_node(_node);
        set_slot(_slot);
        set_name("CPU");
    }
    cpu_info(cpu_info& _ci) {
        deallocate();
        copy_me(_ci);
    }
    ~cpu_info() {
    }

    inline void
        copy_me(cpu_info& _ci) {
        set_type(_ci.get_type());
        set_num(_ci.get_num());
        set_constraints(_ci.get_constraints());
        set_path(_ci.get_path());
        set_module(_ci.get_module());
        set_node(_ci.get_node());
        set_name(_ci.get_name());
        set_slot(_ci.get_slot());
    }

    inline char get_slot()          { return(get_other(0)); }
    inline void set_slot(char slot) { set_other(0, static_cast<int>(slot)); }

    inline void print() {
        cout << "cpu " << num << " " << path << endl;
    }
};

class
io_info: public resource_info {
public:
    io_info(int _num=-1, const char *_path=NULL,
            int _module=-1, int _ioslot=-1) {
        deallocate();
        set_type(R_HIPPI);
        set_num(_num);
        // ci = new constraint_info;
        set_path(_path);
        set_module(_module);
        set_node(-1);
        set_name("HIPPI");
        set_ioslot(_ioslot);
    }
    io_info(io_info& _ii) {
        deallocate();
        copy_me(_ii);
    }
    ~io_info() {
    }

    inline void
        copy_me(io_info& _ii) {
        set_type(_ii.get_type());
        set_num(_ii.get_num());
        set_constraints(_ii.get_constraints());
        set_path(_ii.get_path());
        set_module(_ii.get_module());
        set_node(_ii.get_node());
        set_name(_ii.get_name());
        set_ioslot(_ii.get_ioslot());
    }

    inline int get_ioslot() { return(get_other(2)); }
    void set_ioslot(int s);

    int get_node_num(int s);

    inline void print() {
        cout << "io " << num << " " << path;
        cout << " (n" << get_node_num(0);
        cout << ",n" << get_node_num(1);
        cout << ")" << endl;
    }

protected:
    static const int num_other_used = 3;
};

class
hippi_info: public io_info {
public:
    hippi_info(int _num=-1, const char *_path=NULL,
               int _module=-1, int _ioslot=-1, int _max_jobs=0) {
        deallocate();
        set_type(R_HIPPI);
        set_num(_num);
        // ci = new constraint_info;
        set_path(_path);
        set_module(_module);
        set_node(-1);
        set_name("HIPPI");
        set_ioslot(_ioslot);
        set_max_jobs(_max_jobs);
    }
    hippi_info(hippi_info& _hi) {
        deallocate();
        set_type(R_HIPPI);
        set_num(_hi.get_num());
        // ci = new constraint_info;
        set_constraints(_hi.get_constraints());
        set_path(_hi.get_path());
        set_module(_hi.get_module());
        set_node(_hi.get_node());
        set_name(_hi.get_name());
        set_ioslot(_hi.get_ioslot());
        set_max_jobs(_hi.get_max_jobs());
    }
    ~hippi_info() {
    }

    inline int get_max_jobs()       { return(get_other(num_other_used)); }
    inline void set_max_jobs(int j) { set_other(num_other_used, j); }

    inline void print() {
        cout << "hip " << num << " " << path;
        cout << " (n" << get_node_num(0);
        cout << ",n" << get_node_num(1);
        cout << ") max " << get_max_jobs() << " jobs" << endl;;
    }
};

#ifdef _USE_OLD_HIPPI_INFO_
class
hippi_info: public resource_info {
public:
    hippi_info(int _num=-1, const char *_path=NULL,
               int _module=-1, int _ioslot=-1, int _max_jobs=0) {
        deallocate();
        set_type(R_HIPPI);
        set_num(_num);
        ci = new constraint_info;
        set_path(_path);
        set_module(_module);
        set_node(-1);
        set_name("HIPPI");
        set_ioslot(_ioslot);
        set_max_jobs(_max_jobs);
    }
    hippi_info(hippi_info& _hi) {
        deallocate();
        set_type(R_HIPPI);
        set_num(_hi.get_num());
        ci = new constraint_info;
        set_constraints(_hi.get_constraints());
        set_path(_hi.get_path());
        set_module(_hi.get_module());
        set_node(_hi.get_node());
        set_name(_hi.get_name());
        set_ioslot(_hi.get_ioslot());
        set_max_jobs(_hi.get_max_jobs());
    }
    ~hippi_info() {
    }

    inline int get_ioslot()         { return(get_other(0)); }
    inline int get_max_jobs()       { return(get_other(1)); }
    inline void set_ioslot(int s)   { set_other(0, s); }
    inline void set_max_jobs(int j) { set_other(1, j); }

    inline void print() {
        cout << "hip " << num << " path " << path << endl;
        cout << "     module " << module;
        cout << " node " << get_node();
        cout << " ioslot " << get_ioslot();
        cout << " max " << get_max_jobs() << " jobs";
        cout << endl;
    }
};
#endif

class
mem_info: public resource_info {
public:
    mem_info(int _num=-1, const char *_path=NULL,
             int _module=-1, int _node=-1) {
        deallocate();
        set_type(R_MEMORY);
        set_num(_num);
        // ci = new constraint_info;
        set_path(_path);
        set_module(_module);
        set_node(_node);
        set_name("MEMORY");
    }
    mem_info(mem_info& _mi) {
        deallocate();
        copy_me(_mi);
    }
    ~mem_info() {
    }

    inline void
        copy_me(mem_info& _mi) {
        set_type(_mi.get_type());
        set_num(_mi.get_num());
        set_constraints(_mi.get_constraints());
        set_path(_mi.get_path());
        set_module(_mi.get_module());
        set_node(_mi.get_node());
        set_name(_mi.get_name());
    }

    inline void print() {
        cout << "mem " << num << " " << path << endl;
    }
};

//
// other resources (can't these be somewhere else?)
//
class router_info {
public:
    router_info() {
        int i;
        meta = false;
        module = router_num = -1;
        for (i = 0; i < num_nodes; i++) {
            set_node(i, -1);
        }
        reset_node_cursor();
        for (i = 0; i < num_routers; i++) {
            set_router(i, -1);
        }
        reset_router_cursor();
    }
    ~router_info() {
    }

    bool meta;      // is this a metarouter?
    int module;     // module number
    int router_num; // router number within module

    static const int num_nodes = 2;   // 2 nodes for routers, 0 for metarouters
    static const int num_routers = 4; // always 4 routers

    inline void reset_node_cursor() { node_cursor = -1; }
    int add_node(int i);
    int set_node(int s, int i);
    int get_node(int s);

    inline void reset_router_cursor() { router_cursor = -1; }
    int add_router(int i);
    int set_router(int s, int i);
    int get_router(int s);

private:
    // allocatable resources "connected" to this node
    int node_cursor, node[num_nodes];

    // other resources "connected" to this node
    int router_cursor, router[num_routers];
};

//
// consider a node equivalent to the HUB it is connected to
//
class node_info {
public:
    node_info() {
        num_resource[R_CPU] = num_cpus;
        num_resource[R_HIPPI] = num_hips;
        num_resource[R_MEMORY] = num_mems;

        int i;
        num = module = node_num = -1;
        for (i = 0; i < num_cpus; i++) {
            set_cpu(i, -1);
            resource[R_CPU][i] = -1;
        }
        reset_cpu_cursor();
        for (i = 0; i < num_hips; i++) {
            set_hippi(i, -1);
            resource[R_HIPPI][i] = -1;
        }
        reset_hippi_cursor();
        for (i = 0; i < num_mems; i++) {
            set_memory(i, -1);
            resource[R_MEMORY][i] = -1;
        }
        reset_memory_cursor();
        for (i = 0; i < num_routers; i++) {
            set_router(i, -1);
        }
        reset_router_cursor();
        for (i = 0; i < R_NUM_RESOURCES; i++) {
            allocated[i] = 0;
        }
    }
    ~node_info() {
    }

    int num;      // "global" number
    int module;   // module number
    int node_num; // node number within module

    static const int num_cpus = 2;    // always 2 cpus
    static const int num_hips = 1;    // at most 1 hippi device
    static const int num_mems = 1;    // always 1 memory
    static const int num_routers = 1; // always 1 router
    // why the F*C*can't i use static const and initialize this?!?!?
    int num_resource[R_NUM_RESOURCES];

    inline int get_resource_num(int res, int s) {
        return(resource[res][s]);
    }

    inline void reset_cpu_cursor() { cpu_cursor = -1; }
    int add_cpu(int i);
    int set_cpu(int s, int i);
    int getCpu(int s);

    inline void reset_hippi_cursor() { hippi_cursor = -1; }
    int add_hippi(int i);
    int set_hippi(int s, int i);
    int get_hippi(int s);

    inline void reset_memory_cursor() { memory_cursor = -1; }
    int add_memory(int i);
    int set_memory(int s, int i);
    int get_memory(int s);

    inline void reset_router_cursor() { router_cursor = -1; }
    int add_router(int i);
    int set_router(int s, int i);
    int get_router(int s);

    inline int allocate_resource(int i)      { return(++allocated[i]); }
    inline int get_allocated_resource(int i) { return(allocated[i]); }
    inline int get_total_allocated()         { return(allocated[0] +
                                                      allocated[1] +
                                                      allocated[2]); }

    inline void print() {
        int i;
        if (num != -1) {
            cout << "NODE " << num << ":" << endl;
            for (i = 0; i < num_cpus; i++) {
                if (cpu[i] != -1) {
                    cout << "   cpu " << i << " (" << cpu[i] << ")" << endl;
                }
            }
            for (i = 0; i < num_hips; i++) {
                if (hippi[i] != -1) {
                    cout << "   hippi " << i << " (" << hippi[i] << ")" << endl;
                }
            }
            for (i = 0; i < num_mems; i++) {
                if (memory[i] != -1) {
                    cout << "   memory " << i << " (" << memory[i] << ")" << endl;
                }
            }
            for (i = 0; i < num_routers; i++) {
                if (router[i] != -1) {
                    cout << "   router " << i << " (" << router[i] << ")" << endl;
                }
            }
        }
    }

private:
    // allocatable resources "connected" to this node
    int cpu_cursor, cpu[num_cpus];
    int hippi_cursor, hippi[num_hips];
    int memory_cursor, memory[num_mems];

    // other resources "connected" to this node
    int router_cursor, router[num_routers];

    // how many such resources are actually allocated
    int allocated[R_NUM_RESOURCES];
    int resource[R_NUM_RESOURCES][5];
};

class module_info {
public:
    module_info() {
        int i;
        num = -1;
        for (i = 1; i <= num_nodes; i++) {
            set_node(i, -1);
        }
        reset_node_cursor();
        for (i = 0; i < R_NUM_RESOURCES; i++) {
            allocated[i] = 0;
        }
    }
    ~module_info() {
    }
    int num;       // "global" number

    static const int num_nodes = 4; // always 4 nodes per module

    inline void reset_node_cursor() { node_cursor = -1; }
    int add_node(int i);
    int set_node(int s, int i);
    int get_node(int s);
    int get_priority_node(int s);

    inline int allocate_resource(int i)      { return(++allocated[i]); }
    inline int get_allocated_resource(int i) { return(allocated[i]); }
    inline int get_total_allocated()         { return(allocated[0] +
                                                      allocated[1] +
                                                      allocated[2]); }
    inline void print() {
        int i;
        if (num != -1) {
            cout << "MODULE " << num << ":" << endl;
            for (i = 0; i < num_nodes; i++) {
                if (node[i] != -1) {
                    cout << "   node " << i+1 << " (" << node[i] << ")" << endl;
                }
            }
        }
    }

private:
    int node_cursor, node[num_nodes];

    // how many real resources are actually allocated
    int allocated[R_NUM_RESOURCES];

    // list of nodes sorted by number of resources allocated to them
    int node_priority[num_nodes];
};

#endif /*** __MD_REQUEST_H__ ***/
