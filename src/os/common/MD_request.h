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



#ifndef __MD_TRU64_REQUEST_H__
#define __MD_TRU64_REQUEST_H__

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef __APPLE__
#include <sys/procfs.h>
#include <stropts.h>
#endif

#include "internal/new.h"

//
// REQUIRED: resource_type
//
typedef enum _rt {
    // allocatable resources
    R_CPU=0,
    R_NUM_RESOURCES,
    // other resources
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
        set_name("");
    }
    resource_info(resource_info& ri) {
        deallocate();
        ci = new constraint_info;
        path = NULL;
        copy_me(ri);
    }
    virtual ~resource_info() {
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

#endif // ! __MD_TRU64_REQUEST_H__
