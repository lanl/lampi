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

#include "internal/mpi.h"
#include "internal/mpif.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Type_indexed = PMPI_Type_indexed
#endif

int PMPI_Type_indexed(int count, int *blocklength_array, int *disp_array,
		      MPI_Datatype mtype_old, MPI_Datatype *mtype_new)
{
    int i, rc;
    MPI_Datatype *type_array;
    MPI_Aint *byte_disps;
    ULMType_t *oldtype = mtype_old;
    ULMType_t *newtype, *t;

    if (mtype_new == NULL) {
        ulm_err(("Error: MPI_Type_indexed: Invalid newtype pointer\n"));
        rc = MPI_ERR_TYPE;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    /* check count, return an "empty" datatype if count is zero */
    if (count < 0) {
        ulm_err(("Error: MPI_Type_indexed: count %d is invalid\n", count));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count == 0) {
        newtype = (ULMType_t *) ulm_malloc(sizeof(ULMType_t));
        if (newtype == NULL) {
            ulm_err(("Error: MPI_Type_indexed: Out of memory\n"));
            rc = MPI_ERR_TYPE;
            _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
            return rc;
        }

        newtype->isbasic = 0;
        newtype->layout = CONTIGUOUS;
        newtype->num_pairs = 0;
        newtype->extent = 0;
        newtype->lower_bound = 0;
        newtype->type_map = NULL;
        newtype->committed = 0;
        newtype->ref_count = 1;

        *mtype_new = newtype;

        /* save "envelope" information */
        newtype->envelope.combiner = MPI_COMBINER_INDEXED;
        newtype->envelope.nints = 1;
        newtype->envelope.naddrs = 0;
        newtype->envelope.ndatatypes = 1;
        newtype->envelope.iarray = (int *) ulm_malloc(sizeof(int));
        newtype->envelope.aarray = NULL;
        newtype->envelope.darray =
            (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
        newtype->envelope.iarray[0] = count;
        newtype->envelope.darray[0] = mtype_old;
        t = mtype_old;
        fetchNadd(&(t->ref_count), 1);

        if (_mpi.fortran_layer_enabled) {
            newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
        }

        return MPI_SUCCESS;
    }

    /* create type array containing count copies of mtype_old */
    type_array = (MPI_Datatype *) ulm_malloc(count * sizeof(MPI_Datatype));
    if (type_array == NULL) {
        ulm_err(("Error: MPI_Type_indexed: failure in ulm_malloc\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }
    for (i = 0; i < count; i++)
        type_array[i] = mtype_old;

    /* create array of displacements in bytes */
    byte_disps = ulm_malloc(count * sizeof(MPI_Aint));
    if (type_array == NULL) {
        ulm_err(("Error: MPI_Type_indexed: failure in ulm_malloc\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }
    for (i = 0; i < count; i++)
        byte_disps[i] = disp_array[i] * oldtype->extent;

    /* create new datatype */
    rc = MPI_Type_struct(count, blocklength_array, byte_disps,
                         type_array, mtype_new);

    /* free heap memory */
    ulm_free(type_array);
    ulm_free(byte_disps);

    if (rc != MPI_SUCCESS) {
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
    }

    /* save "envelope" information */
    newtype = *mtype_new;
    for (i = 0; i < newtype->envelope.ndatatypes; i++) {
        t = newtype->envelope.darray[i];
        fetchNadd(&(t->ref_count), -1);
    }
    if (newtype->envelope.iarray)
        ulm_free(newtype->envelope.iarray);
    if (newtype->envelope.aarray)
        ulm_free(newtype->envelope.aarray);
    if (newtype->envelope.darray)
        ulm_free(newtype->envelope.darray);
    newtype->envelope.combiner = MPI_COMBINER_INDEXED;
    newtype->envelope.nints = 2 * count + 1;
    newtype->envelope.naddrs = 0;
    newtype->envelope.ndatatypes = 1;
    newtype->envelope.iarray =
        (int *) ulm_malloc(newtype->envelope.nints * sizeof(int));
    newtype->envelope.aarray = NULL;
    newtype->envelope.darray =
        (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
    newtype->envelope.iarray[0] = count;
    for (i = 1; i <= count; i++) {
        newtype->envelope.iarray[i] = blocklength_array[i - 1];
        newtype->envelope.iarray[i + count] = disp_array[i - 1];
    }
    newtype->envelope.darray[0] = mtype_old;
    t = mtype_old;
    fetchNadd(&(t->ref_count), 1);

    return rc;
}
