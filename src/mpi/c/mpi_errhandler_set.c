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



#include "internal/mpi.h"

#pragma weak MPI_Errhandler_set = PMPI_Errhandler_set
int PMPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler handler)
{
    errhandler_t *handler_new;
    errhandler_t *handler_old;
    int index_new, index_old;

    ulm_comm_lock(comm);

    index_new = (int) handler;
    ulm_get_errhandler_index(comm, &index_old);
    handler_old = _mpi_ptr_table_lookup(_mpi.errhandler_table, index_old);
    handler_new = _mpi_ptr_table_lookup(_mpi.errhandler_table, index_new);

    _mpi_lock(&(handler_old->lock));
    handler_old->refcount--;
    if (handler_old->freed && handler_old->refcount == 0) {
	_mpi_unlock(&(handler_old->lock));
	_mpi_ptr_table_free(_mpi.errhandler_table, index_old);
    } else {
        _mpi_unlock(&(handler_old->lock));
    }

    _mpi_lock(&(handler_new->lock));
    handler_new->refcount++;
    _mpi_unlock(&(handler_new->lock));

    ulm_set_errhandler_index(comm, index_new);

    ulm_comm_unlock(comm);

    return MPI_SUCCESS;
}
