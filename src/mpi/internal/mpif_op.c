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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * MPI Fortran: operation table
 *
 * In fortran, MPI operations are integers, so we need to maintain
 * a table of MPI_Op pointers.
 */

#include "internal/mpif.h"

/*
 * Allocate and initialize the op table
 */
ptr_table_t *_mpif_create_op_table(void)
{
    ptr_table_t *table;

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpif_create_op_table:\n");
    }

    table = ulm_malloc(sizeof(ptr_table_t));
    if (table == NULL) {
	return NULL;
    }
    memset(table, 0, sizeof(ptr_table_t));
    ATOMIC_LOCK_INIT(table->lock);

    /*
     * initialization
     *
     * !!! WARNING !!!  keep this in sync with  mpif.h
     */

    if (_mpi_ptr_table_add(table, MPI_MAX) < 0 ||
	_mpi_ptr_table_add(table, MPI_MIN) < 0 ||
	_mpi_ptr_table_add(table, MPI_SUM) < 0 ||
	_mpi_ptr_table_add(table, MPI_PROD) < 0 ||
	_mpi_ptr_table_add(table, MPI_MAXLOC) < 0 ||
	_mpi_ptr_table_add(table, MPI_MINLOC) < 0 ||
	_mpi_ptr_table_add(table, MPI_BAND) < 0 ||
	_mpi_ptr_table_add(table, MPI_BOR) < 0 ||
	_mpi_ptr_table_add(table, MPI_BXOR) < 0 ||
	_mpi_ptr_table_add(table, MPI_LAND) < 0 ||
	_mpi_ptr_table_add(table, MPI_LOR) < 0 ||
	_mpi_ptr_table_add(table, MPI_LXOR) < 0) {
	ulm_free(table);
	return NULL;
    }

    return table;
}
