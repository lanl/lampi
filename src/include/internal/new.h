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



#ifndef _ULM_INTERNAL_NEW
#define _ULM_INTERNAL_NEW

#include <stdio.h>
#include <stdlib.h>

/*
 * Set ULM_NEW_DEBUG_LEVEL to
 * 0 for no checking
 * 1 for basic error checking
 * 2 for more error checking
 */

#define ULM_NEW_DEBUG_LEVEL 2

/*
 * new and (array) delete with error checking
 */

template <class T> inline T* _ulm_new(ssize_t nitems,
                                      int debug_level,
                                      char *file,
                                      int line)
{
    T *addr = NULL;

    if (debug_level > 1) {
        if (nitems <= 0) {
            fprintf(stderr,
                    "%s:%d: Warning: ulm_new: "
                    "Request for %ld items of size %ld\n",
                    file, line, (long) nitems, (long) sizeof(T));
            fflush(stderr);
        }
    }

    addr = new T[nitems];
    if (debug_level > 0) {
        if (addr == NULL) {
            fprintf(stderr,
                    "%s:%d: Error: ulm_new: "
                    "Request for %ld items of size %ld failed\n",
                    file, line, (long) nitems, (long) sizeof(T));
            fflush(stderr);
        }
    }

    return addr;
}


template <class T> inline void _ulm_delete(T *addr,
                                           int debug_level,
                                           char *file,
                                           int line)
{
    if (debug_level > 1 && addr == NULL) {
        fprintf(stderr,
                "%s:%d: Warning: ulm_free: Invalid pointer %p\n",
                file, line, addr);
        fflush(stderr);
    }
    delete [] addr;
}


/*
 * Macros to actually use
 */

#define ulm_delete(ADDR) \
    do { _ulm_delete((ADDR), ULM_NEW_DEBUG_LEVEL, __FILE__, __LINE__); ADDR = NULL; } while (0)


#define ulm_new(TYPE,NITEMS) \
    _ulm_new<TYPE>(NITEMS, ULM_NEW_DEBUG_LEVEL, __FILE__, __LINE__)


#endif /* _ULM_INTERNAL_NEW */
