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
#include "internal/mpif.h"

#pragma weak MPI_Type_hvector = PMPI_Type_hvector
int PMPI_Type_hvector(int count, int blocklength, MPI_Aint stride,
		      MPI_Datatype mtype_old, MPI_Datatype *mtype_new)
{
    ULMType_t *oldtype = mtype_old;
    ULMType_t *newtype, *t;
    int i, j, k, typemap_i;
    ssize_t typemap_off, new_offset, current_offset;
    size_t new_size, current_size;
    int rc;

    if (mtype_new == NULL) {
        ulm_err(("Error: MPI_Type_hvector: Invalid newtype pointer\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count < 0) {
        ulm_err(("Error: MPI_Type_hvector: count %d is invalid\n", count));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count == 0) {
        newtype = (ULMType_t *) ulm_malloc(sizeof(ULMType_t));
        if (newtype == NULL) {
            ulm_err(("Error: MPI_Type_hvector: Out of memory\n"));
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
        newtype->envelope.combiner = MPI_COMBINER_HVECTOR;
        newtype->envelope.nints = 2;
        newtype->envelope.naddrs = 1;
        newtype->envelope.ndatatypes = 1;
        newtype->envelope.iarray = (int *) ulm_malloc(2 * sizeof(int));
        newtype->envelope.aarray =
            (MPI_Aint *) ulm_malloc(sizeof(MPI_Aint));
        newtype->envelope.darray =
            (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
        newtype->envelope.iarray[0] = count;
        newtype->envelope.iarray[1] = blocklength;
        newtype->envelope.aarray[0] = stride;
        newtype->envelope.darray[0] = mtype_old;
        t = mtype_old;
        fetchNadd(&(t->ref_count), 1);

        if (_mpi.fortran_layer_enabled) {
            newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
        }

        return MPI_SUCCESS;
    }

    /* Allocate newtype */
    newtype = ulm_malloc(sizeof(ULMType_t));
    if (newtype == NULL) {
        ulm_err(("Error: MPI_Type_hvector: Out of memory\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    /* Initialize newtype */
    newtype->isbasic = 0;
    newtype->layout = NON_CONTIGUOUS;
    newtype->committed = 0;
    newtype->ref_count = 1;

    /* calculate the number of entries needed for the type_map */
    typemap_off = 0;
    typemap_i = 0;
    current_offset = current_size = 0;
    for (i = 0; i < count; i++) {
        for (j = 0; j < blocklength; j++) {
            for (k = 0; k < oldtype->num_pairs; k++) {
                new_size = oldtype->type_map[k].size;
                new_offset = oldtype->type_map[k].offset + typemap_off;
                if ((typemap_i != 0)
                    && (current_size + current_offset == new_offset)) {
                    /* consolidate with previous type_map entry */
                    current_size += new_size;
                } else {
                    /* create new type_map entry */
                    current_size = new_size;
                    current_offset = new_offset;
                    typemap_i++;
                }
            }
            typemap_off += oldtype->extent;
        }
        typemap_off += stride - blocklength * oldtype->extent;
    }
    newtype->num_pairs = typemap_i;

    newtype->type_map = (ULMTypeMapElt_t *) ulm_malloc(newtype->num_pairs *
                                                       sizeof
                                                       (ULMTypeMapElt_t));
    if (newtype->type_map == NULL) {
        ulm_err(("Error: MPI_Type_hvector: Out of memory\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    /* save "envelope" information */
    newtype->envelope.combiner = MPI_COMBINER_HVECTOR;
    newtype->envelope.nints = 2;
    newtype->envelope.naddrs = 1;
    newtype->envelope.ndatatypes = 1;
    newtype->envelope.iarray = (int *) ulm_malloc(2 * sizeof(int));
    newtype->envelope.aarray = (MPI_Aint *) ulm_malloc(sizeof(MPI_Aint));
    newtype->envelope.darray =
        (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
    newtype->envelope.iarray[0] = count;
    newtype->envelope.iarray[1] = blocklength;
    newtype->envelope.aarray[0] = stride;
    newtype->envelope.darray[0] = mtype_old;
    t = mtype_old;
    fetchNadd(&(t->ref_count), 1);

    /* Fill in new datatype's type_map, and calculate the
     * lower/upper bound for the new datatype */
    typemap_off = 0;
    typemap_i = 0;
    for (i = 0; i < count; i++) {
        for (j = 0; j < blocklength; j++) {
            for (k = 0; k < oldtype->num_pairs; k++) {
                new_size = oldtype->type_map[k].size;
                new_offset = oldtype->type_map[k].offset + typemap_off;
                if ((typemap_i != 0)
                    && (newtype->type_map[typemap_i - 1].size +
                        newtype->type_map[typemap_i - 1].offset ==
                        new_offset)) {
                    /* consolidate with previous type_map entry */
                    newtype->type_map[typemap_i - 1].size += new_size;
                } else {
                    /* create new type_map entry */
                    newtype->type_map[typemap_i].size = new_size;
                    newtype->type_map[typemap_i].offset = new_offset;
                    typemap_i++;
                }
            }
            typemap_off += oldtype->extent;
        }
        typemap_off += stride - blocklength * oldtype->extent;
    }

    /* calculate the new datatype's extent */
    if (stride < 0)
        stride *= -1;
    newtype->extent = (count - 1) * stride + blocklength * oldtype->extent
        - oldtype->lower_bound;
    newtype->lower_bound = oldtype->lower_bound;

    /* mark the datatype as contiguous if it clearly is...  */
    if (_MPI_MARK_AS_CONTIGUOUS) {
        if (((newtype->num_pairs == 0) &&
             (newtype->extent == 0)) ||
            ((newtype->num_pairs == 1) &&
             (newtype->extent == newtype->type_map[0].size)) ) {
            newtype->layout = CONTIGUOUS;
        }
    }

    *mtype_new = newtype;

    if (_mpi.fortran_layer_enabled) {
        newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
    }

    return MPI_SUCCESS;
}
