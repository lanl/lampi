/*
 * Copyright 2002-2005. The Regents of the University of
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
#pragma weak MPI_Type_create_hindexed = PMPI_Type_create_hindexed
#pragma weak MPI_Type_hindexed = PMPI_Type_hindexed
#endif


#define ENABLE_CREATE_FROM_BASIC_TYPE 1
#define DO_TYPE_DUMP 0
#define SWAP(a,b)           \
    do {                    \
        ssize_t c = (a);    \
        (a) = (b);          \
        (b) = c;            \
    } while (0)

static int create_from_basic_type(int, int *, MPI_Aint *,
                                  MPI_Datatype, MPI_Datatype *);
static void type_dump(ULMType_t *type);



int PMPI_Type_create_hindexed(int count,
                              int *blocklength_array,
                              MPI_Aint *disp_array,
                              MPI_Datatype mtype_old,
                              MPI_Datatype *mtype_new)
{
    int i, rc;
    MPI_Datatype *type_array;
    ULMType_t *newtype, *t;

    if (count < 0) {
        ulm_err(("Error: MPI_Type_hindexed: count %d is invalid\n",
                 count));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }

    if (count == 0) {
        newtype = (ULMType_t *) ulm_malloc(sizeof(ULMType_t));
        if (newtype == NULL) {
            ulm_err(("Error: MPI_Type_hindexed: Out of memory\n"));
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
        newtype->envelope.combiner = MPI_COMBINER_HINDEXED;
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

    /* Optimized special case if mtype_old is a basic type */

    if (ENABLE_CREATE_FROM_BASIC_TYPE) {
        if (((ULMType_t *) mtype_old)->isbasic) {
            return create_from_basic_type(count,
                                          blocklength_array,
                                          disp_array,
                                          mtype_old,
                                          mtype_new);
        }
    }    

    /* create type array containing count copies of mtype_old */
    type_array = (MPI_Datatype *) ulm_malloc(count * sizeof(MPI_Datatype));
    if (type_array == NULL) {
        ulm_err(("Error: MPI_Type_indexed: failure in calloc\n"));
        rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
        return rc;
    }
    for (i = 0; i < count; i++)
        type_array[i] = mtype_old;

    /* create new datatype */
    rc = MPI_Type_struct(count, blocklength_array, disp_array,
                         type_array, mtype_new);

    /* free heap memory */
    ulm_free(type_array);

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
    newtype->envelope.combiner = MPI_COMBINER_HINDEXED;
    newtype->envelope.nints = count + 1;
    newtype->envelope.naddrs = count;
    newtype->envelope.ndatatypes = 1;
    newtype->envelope.iarray =
        (int *) ulm_malloc(newtype->envelope.nints * sizeof(int));
    newtype->envelope.aarray =
        (MPI_Aint *) ulm_malloc(newtype->envelope.naddrs *
                                sizeof(MPI_Aint));
    newtype->envelope.darray =
        (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
    newtype->envelope.iarray[0] = count;
    for (i = 0; i < count; i++) {
        newtype->envelope.iarray[i + 1] = blocklength_array[i];
        newtype->envelope.aarray[i] = disp_array[i];
    }
    newtype->envelope.darray[0] = mtype_old;

    t = mtype_old;
    fetchNadd(&(t->ref_count), 1);

    if (DO_TYPE_DUMP) {
        type_dump(newtype);
    }

    return rc;
}


int PMPI_Type_hindexed(int count,
                       int *blocklength_array,
                       MPI_Aint *disp_array,
                       MPI_Datatype mtype_old,
                       MPI_Datatype *mtype_new)
{
    return PMPI_Type_create_hindexed(count,
                                     blocklength_array,
                                     disp_array,
                                     mtype_old,
                                     mtype_new);
}


/*
 * Special case if mtype_old is basic
 */
static int create_from_basic_type(int count,
                                  int *blocklength_array,
                                  MPI_Aint *disp_array,
                                  MPI_Datatype mtype_old,
                                  MPI_Datatype *mtype_new)
{
    int i;
    size_t size;
    ssize_t lb, ub;
    ULMType_t *newtype;
    ULMType_t *oldtype = (ULMType_t *) mtype_old;

    /* create new type */

    newtype = (ULMType_t *) ulm_malloc(sizeof(ULMType_t));
    newtype->type_map = (ULMTypeMapElt_t *) ulm_malloc(count * sizeof(ULMTypeMapElt_t));

    lb = disp_array[0];
    ub = disp_array[0] + blocklength_array[i] * oldtype->extent;
    if (ub < lb) {
        SWAP(lb, ub);
    }

    for (i = 0; i < count; i++) {
        ssize_t l = disp_array[i];
        ssize_t u = disp_array[i] + blocklength_array[i] * oldtype->extent;

        if (u < l) {
            SWAP(l, u);
        }
        if (ub < u) {
            ub = u;
        }
        if (lb > l) {
            lb = l;
        }
        newtype->type_map[i].size = (size_t) (blocklength_array[i] * oldtype->extent);
        newtype->type_map[i].offset = (ssize_t) disp_array[i];
        newtype->type_map[i].seq_offset = size + newtype->type_map[i].size;
        size += newtype->type_map[i].size;
    }
    newtype->lower_bound = lb;
    newtype->extent = ub - lb;
    newtype->packed_size = 0;
    newtype->num_pairs = count;
    newtype->layout = NON_CONTIGUOUS;
    newtype->isbasic = 0;
    newtype->num_primitives = 0;
    newtype->second_primitive_offset = 0;
    newtype->op_index = 0;
    newtype->fhandle = -1;
    newtype->ref_count = 1;
    newtype->committed = 0;
    if (_mpi.fortran_layer_enabled) {
        newtype->fhandle = _mpi_ptr_table_add(_mpif.type_table, newtype);
    }

    *mtype_new = (MPI_Datatype) newtype;

    /* save "envelope" information */

    newtype->envelope.combiner = MPI_COMBINER_HINDEXED;
    newtype->envelope.nints = count + 1;
    newtype->envelope.naddrs = count;
    newtype->envelope.ndatatypes = 1;
    newtype->envelope.iarray = (int *) ulm_malloc(newtype->envelope.nints * sizeof(int));
    newtype->envelope.aarray = (MPI_Aint *) ulm_malloc(newtype->envelope.naddrs *
                                                       sizeof(MPI_Aint));
    newtype->envelope.darray = (MPI_Datatype *) ulm_malloc(sizeof(MPI_Datatype));
    newtype->envelope.iarray[0] = count;
    for (i = 0; i < count; i++) {
        newtype->envelope.iarray[i + 1] = blocklength_array[i];
        newtype->envelope.aarray[i] = disp_array[i];
    }
    newtype->envelope.darray[0] = mtype_old;

    /* mtype_old is basic so no need to increment its ref count */

    if (DO_TYPE_DUMP) {
        type_dump(newtype);
    }

    return ULM_SUCCESS;
}


/*
 * type_dump - Dump out a datatype to stderr (for debugging)
 *
 * \param type          pointer to datatype
 */
static void type_dump(ULMType_t *type)
{
    int i;

    fprintf(stderr,
            "type_dump: type at %p:\n"
            "\tlower_bound = %ld\n"
            "\textent = %ld\n"
            "\tpacked_size = %ld\n"
            "\tnum_pairs = %d\n"
            "\tlayout = %d\n"
            "\tisbasic = %d\n"
            "\tnum_primitives = %d\n"
            "\tsecond_primitive_offset = %d\n"
            "\top_index = %d\n"
            "\tfhandle = %d\n"
            "\ttype_map =",
            type,
            (long) type->lower_bound,
            (long) type->extent,
            (long) type->packed_size,
            type->num_pairs,
            type->layout,
            type->isbasic,
            type->num_primitives,
            type->second_primitive_offset,
            type->op_index,
            type->fhandle);
    for (i = 0; i < type->num_pairs; i++) {
        fprintf(stderr,
                " (%ld, %ld, %ld)",
                (long) type->type_map[i].size,
                (long) type->type_map[i].offset,
                (long) type->type_map[i].seq_offset);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}
