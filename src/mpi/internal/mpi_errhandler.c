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

/*
 * MPI: error handler table
 */

#include "internal/mpi.h"

static void _mpi_errors_are_fatal(MPI_Comm *comm, int *rc,
				  char *file, int *line)
{
    char errstring[MPI_MAX_ERROR_STRING];
    char *basename;
    int length;

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_errors_are_fatal:\n");
    }

    basename = strrchr(file, '/');
    if (basename == ((char *)NULL) ) {
	basename = file;
    } else {
	basename += 1;
    }

    MPI_Error_string(*rc, errstring, &length);

    _ulm_err("%s:%d: MPI error: %s (%d): MPI errors are fatal\n",
	     basename, *line, errstring, *rc);
    ulm_abort(*comm, *rc);
}


static void _mpi_errors_return(MPI_Comm *comm, int *rc, ...)
{
    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_errors_return:\n");
    }

    return;
}


/*
 * Allocate and initialize the error handler table
 */
ptr_table_t *_mpi_create_errhandler_table(void)
{
    ptr_table_t *table;
    errhandler_t *errhandler;

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_create_errhandler_table:\n");
    }

    table = ulm_malloc(sizeof(ptr_table_t));
    if (table == NULL) {
	return NULL;
    }
    memset(table, 0, sizeof(ptr_table_t));

    /*
     * add default error handlers to the table
     *
     * N.B. ugly code currently relies on
     *
     * MPI_ERRORS_ARE_FATAL = 0
     * MPI_ERRORS_RETURN = 1
     */

    /* MPI_ERRORS_ARE_FATAL */

    errhandler = ulm_malloc(sizeof(errhandler_t));
    if (errhandler == NULL) {
	ulm_free(table);
	return NULL;
    }
    memset(errhandler, 0, sizeof(errhandler_t));
    errhandler->func = (MPI_Handler_function *) _mpi_errors_are_fatal;
    errhandler->freed = 0;
    errhandler->isbasic = 1;
    cLockInit(&(errhandler->lock));
    errhandler->refcount = 1;
    if (_mpi_ptr_table_add(table, errhandler) < 0) {
	ulm_free(table);
	ulm_free(errhandler);
	return NULL;
    }

    /* MPI_ERRORS_RETURN */

    errhandler = ulm_malloc(sizeof(errhandler_t));
    if (errhandler == NULL) {
	ulm_free(table);
	return NULL;
    }
    memset(errhandler, 0, sizeof(errhandler_t));
    errhandler->func = (MPI_Handler_function *) _mpi_errors_return;
    errhandler->freed = 0;
    errhandler->isbasic = 1;
    cLockInit(&(errhandler->lock));
    errhandler->refcount = 1;
    if (_mpi_ptr_table_add(table, errhandler) < 0) {
	ulm_free(table);
	ulm_free(errhandler);
	return NULL;
    }

    return table;
}


/*
 * Call the error handler
 */
void _mpi_errhandler(MPI_Comm comm, int rc, char *file, int line)

{
    errhandler_t *handler;
    int index;

    if (_mpi.finalized) {
        return;
    }

    /* retrieve index into error handler table for this communicator */
    if (ulm_get_errhandler_index((int) comm, &index) != ULM_SUCCESS) {
        comm = MPI_COMM_WORLD;
    }

    if (index < _mpi.errhandler_table->size) {
	handler = (errhandler_t *) _mpi.errhandler_table->addr[index];
	_mpi_lock(&(handler->lock));
	handler->func(&comm, &rc, file, &line);
	_mpi_unlock(&(handler->lock));
    } else {
	ulm_err(("Error: MPI error handler not found: returning\n"));
    }
}
