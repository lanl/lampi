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

#ifndef _ACQUIRE_H_
#define _ACQUIRE_H_

#ifndef _emit_error_msg_and_exit
#define _emit_error_msg_and_exit() \
      ulm_exit((-1, "Fatal error: %s line %d\n", __FILE__, __LINE__));
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "internal/log.h"
#include "internal/new.h"
#include "os/IRIX/request.h"

class
MI_acquire {
public:
    MI_acquire() {
        num_children = 0;
        num_used=0;
    }
    ~MI_acquire() {
        MI_cleanup();
    }

    // typedef for identifying children
    typedef int child_t;

    // enum type describing where the functions should be called from
    enum { from_parent, from_child, from_either, from_both };

protected:
    // all decendants MUST call MI_init() in their constructor
    inline void
        MI_init(int n) {
        num_children = 0;
        reset_num_children(n);
    }

    // all decendants MUST call MI_cleanup() in their destructor
    inline void
        MI_cleanup() {
        if (num_children != 0) {
            ulm_delete(children);
        }
    }

public:
    // virtual inline bool init_parent() { return(true); }
    // virtual inline bool init_child()  { return(true); }
    // virtual void acquire_resource()   { }

    inline int get_num_children()       { return(num_children); }
    inline pid_t get_child_pid(int c)   { return(children[c]); }
    inline int get_child_num(pid_t pid) {
        int i;
        for (i = 0; i < num_children; i++) {
            if (children[i] == pid) {
                return(i);
            }
        }
        return(-1);
    }

    inline void
        reset_num_children(int n) {
        if (num_children != 0) {
            ulm_delete(children);
        }
        num_children = n;
        if (num_children != 0) {
            int i;
            children = ulm_new(pid_t, num_children);
            for (i = 0; i < num_children; i++) {
                children[i] = -1;
            }
        } else {
            children = NULL;
        }
        num_used = 0;
    }

    inline child_t
        register_child(pid_t pid, child_t id) {
        if (!(id < num_children)) {
            _emit_error_msg_and_exit();
        }
        pid_t old_pid = children[id];
        children[id] = pid;
        num_used++;
        if (old_pid != pid) {
            return (-1);
        } else {
            return (id);
        }
    }

    inline child_t
        register_child(pid_t pid) {
        while ((children[num_used] != -1) && (num_used < num_children)) {
            num_used++;
        }
        if (num_used >= num_children) {
            fprintf(stderr,
                    "Warning: number of children exceed requested number..  resetting.\n");
            num_used = 0;
        }
        children[num_used] = pid;
        return(num_used++);
    }

private:
    int num_children;
    int num_used;
    pid_t *children;
};

#ifdef __mips
#include "os/IRIX/MD_acquire.h"
#endif

#endif /*** _ACQUIRE_H_ ***/
