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



#include <stdlib.h>

#include "internal/malloc.h"
#include "internal/mpi.h"
#include "os/atomic.h"

int type_ptrs_free(ULMType_t *type)
{
    /*
     * Free the appropriate "envelope" info, type_map, and then the type.
     *
     * In fact, add them to the freeable pointers table, then if there
     * are no pending messages, free all pointers in the freeable
     * pointer table.
     */

    ptr_table_t *table = _mpi.free_table;
    int i, rc;

    if (type) {
        if (type->isbasic || (fetchNadd(&(type->ref_count), -1) > 1)) {
            /* we don't free basic types ever, and others only when their
             * reference count goes to zero... */
            return MPI_SUCCESS;
        } else {
            /* free the envelope's integer array */
            if ((type->envelope.nints > 0) && (type->envelope.iarray != NULL)
               && (_mpi_ptr_table_add(table, type->envelope.iarray) < 0)) {
                rc = MPI_ERR_INTERN;
                _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                return rc;
            }
            /* free the envelope's address array */
            if ((type->envelope.naddrs > 0) && (type->envelope.aarray != NULL)
               && (_mpi_ptr_table_add(table, type->envelope.aarray) < 0)) {
                rc = MPI_ERR_INTERN;
                _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                return rc;
            }
            /* free the envelope's datatype array after potentially freeing the 
             * referenced datatypes...
             */
            if ((type->envelope.ndatatypes > 0) && (type->envelope.darray != NULL)) {
                for (i = 0; i < type->envelope.ndatatypes; i++) {
                    /* a little bit of recursion never hurt anyone...I hope... */
                    rc = type_ptrs_free((ULMType_t *)type->envelope.darray[i]);
                    if (rc != MPI_SUCCESS) {
                        return rc;
                    }
                }
                if (_mpi_ptr_table_add(table, type->envelope.darray) < 0) {
                    rc = MPI_ERR_INTERN;
                    _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                    return rc;
                }
            }
            /* free the type map */
            if ( (type->type_map!=NULL) &&
                (_mpi_ptr_table_add(table, type->type_map) < 0) ) {
                rc = MPI_ERR_INTERN;
                _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                return rc;
            }
            /* free the datatype structure */
            if ( _mpi_ptr_table_add(table, type) < 0) {
                rc = MPI_ERR_INTERN;
                _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
                return rc;
            }
        }
    }

    return MPI_SUCCESS;
}

#pragma weak MPI_Type_free = PMPI_Type_free
int PMPI_Type_free(MPI_Datatype *mtype)
{
    ULMType_t *type;
    ptr_table_t *table = _mpi.free_table;
    int i, rc;
    int messages_pending;

    if (mtype) {
        type = *mtype;
        if ((rc = type_ptrs_free(type)) != MPI_SUCCESS) {
            return rc;
        }
        *mtype = MPI_DATATYPE_NULL;
    }

    /*
     * free the contents of the freeable pointers table
     */

    ulm_pending_messages(&messages_pending);
    if (!messages_pending) {
        _mpi_lock(&(table->lock));
        for (i = 0; i < table->size; i++) {
            if (table->addr[i]) {
                ulm_free(table->addr[i]);
                table->addr[i] = NULL;
            }
        }
        table->lowest_free = 0;
        table->number_free = table->size;
        _mpi_unlock(&(table->lock));
    }

    return MPI_SUCCESS;
}

