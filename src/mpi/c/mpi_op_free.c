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

#include "internal/mpi.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Op_free = PMPI_Op_free
#endif

int PMPI_Op_free(MPI_Op *mop)
{
    /*
     * Free the user defined operation
     *
     * In fact, add it to the freeable pointers table, then if there
     * are no pending messages, free all pointers in the freeable
     * pointer table.
     */

    ULMOp_t *op;
    ptr_table_t *table = _mpi.free_table;
    int i; 
    int rc = MPI_SUCCESS;
    int messages_pending;

    if (mop) {
        op = *mop;
        if (op) {
            if (op->isbasic) {
                /* we don't free basic ops ever... */
                rc = MPI_SUCCESS;
            } else {
                if (_mpi_ptr_table_add(table, op->func) < 0 ||
                        _mpi_ptr_table_add(table, op) < 0) {
                    rc = MPI_ERR_INTERN;
                    _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                    return rc;
                }
                *mop = MPI_OP_NULL;
            }
        }
    }

    /*
     * free the contents of the freeable pointers table
     */

    ulm_pending_messages(&messages_pending);
    if (!messages_pending) {
        ATOMIC_LOCK(table->lock);
        for (i = 0; i < table->size; i++) {
            if (table->addr[i]) {
                ulm_free(table->addr[i]);
                table->addr[i] = NULL;
            }
        }
        table->lowest_free = 0;
        table->number_free = table->size;
        ATOMIC_UNLOCK(table->lock);
    }


    return rc;
}
