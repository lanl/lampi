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

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Type_struct = PMPI_Type_struct
#endif

int PMPI_Type_struct(int count,
                     int *array_of_blocklengths,
                     MPI_Aint *array_of_displacements,
                     MPI_Datatype *array_of_types,
                     MPI_Datatype *newdatatype)
{
    ULMType_t *newtype, *t;
    ULMType_t **types = (ULMType_t **) array_of_types;
    int i, j, k;
    int mpi_lb_i, mpi_ub_i;
    int min_lb, max_ub;
    int min_disp_i, max_disp_i;
    int typemap_i;
    ssize_t lb, ub, min_disp, max_disp, typemap_off, current_offset,
        new_offset;
    size_t current_size, new_size;
    int rc;

    if (newdatatype == NULL) {
        ulm_err(("Error: MPI_Type_struct: Invalid newtype pointer\n"));
        rc = MPI_ERR_TYPE;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count < 0) {
        ulm_err(("Error: MPI_Type_struct: count %d is invalid\n", count));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count == 0) {
        newtype = (ULMType_t *) ulm_malloc(sizeof(ULMType_t));
        if (newtype == NULL) {
            ulm_err(("Error: MPI_Type_struct: Out of memory\n"));
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

        *newdatatype = newtype;

        /* save "envelope" information */
        newtype->envelope.combiner = MPI_COMBINER_STRUCT;
        newtype->envelope.nints = 1;
        newtype->envelope.naddrs = 0;
        newtype->envelope.ndatatypes = 0;
        newtype->envelope.iarray = (int *) ulm_malloc(sizeof(int));
        newtype->envelope.aarray = NULL;
        newtype->envelope.darray = NULL;
        newtype->envelope.iarray[0] = count;

        if (_mpi.fortran_layer_enabled) {
            newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
        }

        return MPI_SUCCESS;
    }

    /* Allocate new type */
    newtype = ulm_malloc(sizeof(ULMType_t));
    if (newtype == NULL) {
        ulm_err(("Error: MPI_Type_struct: Out of memory\n"));
        rc = MPI_ERR_TYPE;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    /* Initialize newtype */
    newtype->isbasic = 0;
    newtype->op_index = 0;
    newtype->layout = NON_CONTIGUOUS;
    newtype->committed = 0;
    newtype->ref_count = 1;

    /* save "envelope" information */
    newtype->envelope.combiner = MPI_COMBINER_STRUCT;
    newtype->envelope.nints = count + 1;
    newtype->envelope.naddrs = count;
    newtype->envelope.ndatatypes = count;
    newtype->envelope.iarray =
        (int *) ulm_malloc(newtype->envelope.nints * sizeof(int));
    newtype->envelope.aarray =
        (MPI_Aint *) ulm_malloc(newtype->envelope.naddrs *
                                sizeof(MPI_Aint));
    newtype->envelope.darray =
        (MPI_Datatype *) ulm_malloc(newtype->envelope.ndatatypes *
                                    sizeof(MPI_Datatype));
    newtype->envelope.iarray[0] = count;
    for (i = 0; i < count; i++) {
        newtype->envelope.iarray[i + 1] = array_of_blocklengths[i];
        newtype->envelope.aarray[i] = array_of_displacements[i];
        newtype->envelope.darray[i] = array_of_types[i];
        t = array_of_types[i];
        fetchNadd(&(t->ref_count), 1);
    }

    /*
     * Look for MPI_LB, MPI_UB markers, smallest/largest
     * displacements, and save off indices
     */
    mpi_lb_i = -1;
    mpi_ub_i = -1;
    /* initialize min_lb and max_lb to quiet not-so-bright compilers */
    min_lb = 0;
    max_ub = 0;
    min_disp = array_of_displacements[0];
    max_disp = array_of_displacements[0];
    min_disp_i = 0;
    max_disp_i = 0;
    for (i = 0; i < count; i++) {
        if (types[i]->extent == 0) {
            if (types[i]->op_index == -1) {
                if ((mpi_lb_i == -1)
                    || (array_of_displacements[i] < min_lb)) {
                    min_lb = array_of_displacements[i];
                    mpi_lb_i = i;
                }
            } else if (types[i]->op_index == -2) {
                if ((mpi_lb_i == -1)
                    || (array_of_displacements[i] > max_ub)) {
                    max_ub = array_of_displacements[i];
                    mpi_ub_i = i;
                }
            }
        }
        if (types[i]->lower_bound > 0) {
            if ((mpi_lb_i == -1) || (types[i]->lower_bound < min_lb)) {
                min_lb = types[i]->lower_bound;
                mpi_lb_i = i;
            }
        }
        if (array_of_displacements[i] < min_disp) {
            min_disp = array_of_displacements[i];
            min_disp_i = i;
        }
        if (array_of_displacements[i] > max_disp) {
            max_disp = array_of_displacements[i];
            max_disp_i = i;
        }
    }

    /*
     * calculate the new datatype's extent, and set the
     * lower bound
     */
    lb = 0, ub = 0;
    if (mpi_lb_i != -1) {
        lb = min_lb;
    } else {
        lb = array_of_displacements[min_disp_i];
    }
    if (mpi_ub_i != -1) {
        ub = max_ub;
    } else {
        ub = array_of_displacements[max_disp_i]
            + array_of_blocklengths[max_disp_i]
            * types[max_disp_i]->extent;
    }
    /* extent should never be less than zero */
    if (ub < lb) {
        ub = lb;
    }
    newtype->extent = ub - lb;
    newtype->lower_bound = lb;

    /* calculate the number of entries needed for the new type_map */
    typemap_i = 0;
    current_size = current_offset = 0;
    for (i = 0; i < count; i++) {
        if (types[i]->extent > 0) {
            typemap_off = array_of_displacements[i];
            for (j = 0; j < array_of_blocklengths[i]; j++) {
                for (k = 0; k < types[i]->num_pairs; k++) {
                    new_size = types[i]->type_map[k].size;
                    new_offset = types[i]->type_map[k].offset +
                        typemap_off;
                    if ((typemap_i != 0)
                        && (current_size + current_offset == new_offset)) {
                        /* consolidate entries */
                        current_size += new_size;
                        if (_MPI_DTYPE_TRIM
                            && current_size + current_offset > ub) {
                            current_size = ub - current_offset;
                        }
                    } else {
                        if (!_MPI_DTYPE_TRIM
                            || ((new_offset + new_size > lb)
                                && (new_offset < ub))) {
                            /* create new type_map entry if there is still something 
                             * left after possible trimming */
                            if (_MPI_DTYPE_TRIM && new_offset < lb) {
                                new_size -= (lb - new_offset);
                                new_offset = lb;
                            }
                            if (_MPI_DTYPE_TRIM
                                && new_offset + new_size > ub) {
                                new_size = (ub - new_offset);
                            }
                            if (new_size > 0) {
                                current_size = new_size;
                                current_offset = new_offset;
                                typemap_i++;
                            }
                        }
                    }
                }
                typemap_off += types[i]->extent;
            }
        }
    }
    /* more newtype initialization */
    newtype->num_pairs = typemap_i;
    if (newtype->num_pairs > 0) {
        /* allocate the type_map */
        newtype->type_map = (ULMTypeMapElt_t *)
            ulm_malloc(newtype->num_pairs * sizeof(ULMTypeMapElt_t));
        if (newtype->type_map == NULL) {
            ulm_err(("Error: MPI_Type_struct: Out of memory\n"));
            rc = MPI_ERR_TYPE;
            _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
            return rc;
        }

        /*
         * Fill in new datatype's type_map....
         */
        typemap_i = 0;
        for (i = 0; i < count; i++) {
            if (types[i]->extent > 0) {
                typemap_off = array_of_displacements[i];
                for (j = 0; j < array_of_blocklengths[i]; j++) {
                    for (k = 0; k < types[i]->num_pairs; k++) {
                        new_size = types[i]->type_map[k].size;
                        new_offset = types[i]->type_map[k].offset +
                            typemap_off;
                        if ((typemap_i != 0)
                            &&
                            ((newtype->type_map[typemap_i - 1].size +
                              newtype->type_map[typemap_i - 1].offset) ==
                             new_offset)) {
                            /* consolidate entries - trim at ub */
                            newtype->type_map[typemap_i - 1].size +=
                                new_size;
                            if (_MPI_DTYPE_TRIM
                                && (newtype->type_map[typemap_i - 1].size +
                                    newtype->type_map[typemap_i -
                                                      1].offset > ub)) {
                                newtype->type_map[typemap_i - 1].size =
                                    ub - newtype->type_map[typemap_i -
                                                           1].offset;
                            }
                        } else {
                            if (!_MPI_DTYPE_TRIM
                                || ((new_offset + new_size > lb)
                                    && (new_offset < ub))) {
                                /* create new type_map entry if there is still something 
                                 * left after possible trimming */
                                if (_MPI_DTYPE_TRIM && new_offset < lb) {
                                    new_size -= (lb - new_offset);
                                    new_offset = lb;
                                }
                                if (_MPI_DTYPE_TRIM
                                    && new_offset + new_size > ub) {
                                    new_size = (ub - new_offset);
                                }
                                if (new_size > 0) {
                                    newtype->type_map[typemap_i].size =
                                        new_size;
                                    newtype->type_map[typemap_i].offset =
                                        new_offset;
                                    typemap_i++;
                                }
                            }
                        }
                    }
                    typemap_off += types[i]->extent;
                }
            }
        }
    } /* end if newtype->numpairs > 0 */
    else {
        newtype->type_map = NULL;
    }

    /* mark the datatype as contiguous if it clearly is ... */
    if (_MPI_MARK_AS_CONTIGUOUS) {
        if (((newtype->num_pairs == 0) && (newtype->extent == 0))
            || ((newtype->num_pairs == 1)
                && (newtype->extent == newtype->type_map[0].size))) {
            newtype->layout = CONTIGUOUS;
        }
    }

    *newdatatype = newtype;

    if (_mpi.fortran_layer_enabled) {
        newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
    }

    return MPI_SUCCESS;
}
