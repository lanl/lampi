/*
 * Copyright 2002-2004. The Regents of the University of
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * MPI Fortran: type table
 *
 * In fortran, MPI types are integers, so we need to maintain
 * a table of MPI_Datatype pointers.
 */

#include "internal/mpif.h"

/*
 * add a datatype to the fortran handle table and initialize the
 * fhandle member with the value of the handle
 */
static void add_fortran_datatype(ptr_table_t * table, MPI_Datatype type,
                                 int *error)
{
    if (*error == 0) {
        ULMType_t *t = type;

        t->fhandle = _mpi_ptr_table_add(table, type);
        if (t->fhandle < 0) {
            *error = 1;
        }
    }
}

/*
 * Fortran basic datatype initialization.
 * 
 * Returns the allocated and initialized type table.
 */
ptr_table_t *_mpif_create_type_table(void)
{
    ptr_table_t *table;
    int error;

    if (_MPI_DEBUG) {
        _mpi_dbg("_mpif_create_type_table:\n");
    }

    /*
     * allocate space for handle table
     */

    table = ulm_malloc(sizeof(ptr_table_t));
    memset(table, 0, sizeof(ptr_table_t));
    ATOMIC_LOCK_INIT(table->lock);

    /*
     * Now add all fortran accessible types to the handle table in the
     * "correct" order.
     *
     * !!! WARNING !!!  keep this order in sync with  mpif.h
     */

    error = 0;
    add_fortran_datatype(table, MPI_INTEGER, &error);
    add_fortran_datatype(table, MPI_REAL, &error);
    add_fortran_datatype(table, MPI_DOUBLE_PRECISION, &error);
    add_fortran_datatype(table, MPI_COMPLEX, &error);
    add_fortran_datatype(table, MPI_DOUBLE_COMPLEX, &error);
    add_fortran_datatype(table, MPI_LOGICAL, &error);
    add_fortran_datatype(table, MPI_CHARACTER, &error);
    add_fortran_datatype(table, MPI_BYTE, &error);
    add_fortran_datatype(table, MPI_PACKED, &error);
    add_fortran_datatype(table, MPI_2REAL, &error);
    add_fortran_datatype(table, MPI_2DOUBLE_PRECISION, &error);
    add_fortran_datatype(table, MPI_2INTEGER, &error);
    add_fortran_datatype(table, MPI_INTEGER1, &error);
    add_fortran_datatype(table, MPI_INTEGER2, &error);
    add_fortran_datatype(table, MPI_INTEGER4, &error);
    add_fortran_datatype(table, MPI_INTEGER8, &error);
    add_fortran_datatype(table, MPI_REAL2, &error);
    add_fortran_datatype(table, MPI_REAL4, &error);
    add_fortran_datatype(table, MPI_REAL8, &error);
    add_fortran_datatype(table, MPI_LB, &error);
    add_fortran_datatype(table, MPI_UB, &error);
    if (error) {
        ulm_free(table);
        return NULL;
    }

    return table;
}
