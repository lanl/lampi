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

#ifndef LAMPI_INTERNAL_STATE_H
#define LAMPI_INTERNAL_STATE_H

/*
 * LA-MPI run-time state
 */

#include "internal/lampi_state.h"

extern lampiState_t lampiState;


/*
 * Accessor methods for state information
 */

#define STATE(X)        lampiState.(X)
#define SETSTATE(X,Y)  (lampiState.(X) = (Y))

#ifdef __cplusplus

inline int myproc()
{
    return lampiState.global_rank;
}

inline int nprocs()
{
    return lampiState.global_size;
}

inline int myhost()
{
    return lampiState.hostid;
}

inline int nhosts()
{
    return lampiState.nhosts;
}

inline int local_nprocs()
{
    return lampiState.map_host_to_local_size[lampiState.hostid];
}

inline int local_myproc()
{
    return lampiState.local_rank;
}

inline int global_to_local_proc(int x)
{
    return x - lampiState.global_to_local_offset;
}

inline long global_proc_to_host(long x)
{
    return lampiState.map_global_rank_to_host[x];
}

	inline int usethreads()
{
    return lampiState.usethreads;
}

inline int usecrc()
{
    return lampiState.usecrc;
}

inline pathContainer_t *pathContainer()
{
    return lampiState.pathContainer;
}

inline int getMemPoolIndex()
{
    return lampiState.memLocalityIndex;
}

inline bool enforceAffinity()
{
    return (bool) lampiState.enforceAffinity;
}

#endif /* __cplusplus */

#endif /* LAMPI_INTERNAL_STATE_H */
