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
 * MPI Fortran: type table
 *
 * In fortran, MPI types are integers, so we need to maintain
 * a table of MPI_Datatype pointers.
 */

#include "internal/mpif.h"

/*
 * This enum lists the datatypes to create specially for fortran
 * (that we cannot reuse from C)
 */
enum {
    I_INTEGER1 = 0,
    I_INTEGER2,
    I_INTEGER4,
    I_REAL2,
    I_REAL4,
    I_REAL8,
    I_COMPLEX,
    I_DOUBLE_COMPLEX,
    I_2REAL,
    I_2DOUBLE_PRECISION,
    I_2INTEGER,
    NUMBER_OF_FORTRAN_TYPES
};

/*
 * Static space for basic datatypes
 */
static ULMType_t type[NUMBER_OF_FORTRAN_TYPES];
static ULMTypeMapElt_t pair[NUMBER_OF_FORTRAN_TYPES];

/*
 * External C symbols for fortran basic datatypes
 */
MPI_Datatype MPI_INTEGER1 = &(type[I_INTEGER1]);
MPI_Datatype MPI_INTEGER2 = &(type[I_INTEGER2]);
MPI_Datatype MPI_INTEGER4 = &(type[I_INTEGER4]);
MPI_Datatype MPI_REAL2 = &(type[I_REAL2]);
MPI_Datatype MPI_REAL4 = &(type[I_REAL4]);
MPI_Datatype MPI_REAL8 = &(type[I_REAL8]);
MPI_Datatype MPI_COMPLEX = &(type[I_COMPLEX]);
MPI_Datatype MPI_DOUBLE_COMPLEX = &(type[I_DOUBLE_COMPLEX]);
MPI_Datatype MPI_2REAL = &(type[I_2REAL]);
MPI_Datatype MPI_2DOUBLE_PRECISION = &(type[I_2DOUBLE_PRECISION]);
MPI_Datatype MPI_2INTEGER = &(type[I_2INTEGER]);

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
    int i;

    if (_MPI_DEBUG) {
        _mpi_dbg("_mpif_create_type_table:\n");
    }

    /*
     * allocate space for handle table, types, type maps
     */

    table = ulm_malloc(sizeof(ptr_table_t));
    memset(table, 0, sizeof(ptr_table_t));
    memset(type, 0, NUMBER_OF_FORTRAN_TYPES * sizeof(ULMType_t));
    memset(pair, 0, NUMBER_OF_FORTRAN_TYPES * sizeof(ULMTypeMapElt_t));
    ATOMIC_LOCK_INIT(table->lock);

    /*
     * specific initialization
     */

    type[I_INTEGER1].extent = 1;
    type[I_INTEGER1].op_index = _MPI_CHAR_OP_INDEX;

    type[I_INTEGER2].extent = 2;
    type[I_INTEGER2].op_index = _MPI_SHORT_OP_INDEX;

    type[I_INTEGER4].extent = 4;
    type[I_INTEGER4].op_index = _MPI_INT_OP_INDEX;

    type[I_REAL2].extent = 2;
    type[I_REAL2].op_index = _MPI_FLOAT_OP_INDEX;

    type[I_REAL4].extent = 4;
    type[I_REAL4].op_index = _MPI_FLOAT_OP_INDEX;

    type[I_REAL8].extent = 8;
    type[I_REAL8].op_index = _MPI_DOUBLE_OP_INDEX;

    type[I_COMPLEX].extent = 2 * sizeof(float);
    type[I_COMPLEX].op_index = _MPI_SCOMPLEX_OP_INDEX;

    type[I_DOUBLE_COMPLEX].extent = 2 * sizeof(double);
    type[I_DOUBLE_COMPLEX].op_index = _MPI_DCOMPLEX_OP_INDEX;

    type[I_2REAL].extent = 2 * sizeof(float);
    type[I_2REAL].op_index = _MPI_2REAL_OP_INDEX;

    type[I_2DOUBLE_PRECISION].extent = 2 * sizeof(double);
    type[I_2DOUBLE_PRECISION].op_index = _MPI_2DOUBLE_PRECISION_OP_INDEX;

    type[I_2INTEGER].extent = 2 * sizeof(int);
    type[I_2INTEGER].op_index = _MPI_2INT_OP_INDEX;

    /*
     * common initialization
     */

    for (i = 0; i < NUMBER_OF_FORTRAN_TYPES; i++) {
        pair[i].size = type[i].extent;
        pair[i].offset = 0;
        pair[i].seq_offset = 0;
        type[i].type_map = &pair[i];
        type[i].packed_size = type[i].extent;
        type[i].num_pairs = 1;
        type[i].layout = CONTIGUOUS;
        type[i].isbasic = 1;
        type[i].num_primitives = 1;
        type[i].second_primitive_offset = 0;
        type[i].committed = 1;
        type[i].ref_count = 1;
        type[i].envelope.combiner = MPI_COMBINER_NAMED;
        type[i].envelope.nints = 0;
        type[i].envelope.naddrs = 0;
        type[i].envelope.ndatatypes = 0;
    }

    /*
     * pair types are special
     */

    type[I_2INTEGER].num_primitives = 2;
    type[I_2REAL].num_primitives = 2;
    type[I_2DOUBLE_PRECISION].num_primitives = 2;

    type[I_2INTEGER].second_primitive_offset = type[I_2INTEGER].extent - sizeof(int);
    type[I_2REAL].second_primitive_offset = type[I_2REAL].extent - sizeof(float);
    type[I_2DOUBLE_PRECISION].second_primitive_offset = type[I_2DOUBLE_PRECISION].extent - sizeof(double);

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
