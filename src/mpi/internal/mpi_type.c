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

#include <string.h>

#include "internal/mpi.h"

/*
 * Static space for basic datatypes
 */

ULMType_t ulm_type[ULM_NUMBER_OF_TYPES];

static ULMTypeMapElt_t tmap[ULM_NUMBER_OF_TYPES];

/*
 * Basic datatype initialization
 */
int _mpi_init_datatypes(void)
{
    int i;

    /*
     * specific initialization
     */

    ulm_type[ULM_CHAR].extent = sizeof(signed char);
    ulm_type[ULM_CHAR].op_index = _MPI_CHAR_OP_INDEX;

    ulm_type[ULM_SHORT].extent = sizeof(signed short int);
    ulm_type[ULM_SHORT].op_index = _MPI_SHORT_OP_INDEX;

    ulm_type[ULM_INT].extent = sizeof(signed int);
    ulm_type[ULM_INT].op_index = _MPI_INT_OP_INDEX;

    ulm_type[ULM_LONG].extent = sizeof(signed long int);
    ulm_type[ULM_LONG].op_index = _MPI_LONG_OP_INDEX;

    ulm_type[ULM_UNSIGNED_CHAR].extent = sizeof(unsigned char);
    ulm_type[ULM_UNSIGNED_CHAR].op_index = _MPI_UNSIGNED_CHAR_OP_INDEX;

    ulm_type[ULM_UNSIGNED_SHORT].extent = sizeof(unsigned short int);
    ulm_type[ULM_UNSIGNED_SHORT].op_index = _MPI_UNSIGNED_SHORT_OP_INDEX;

    ulm_type[ULM_UNSIGNED].extent = sizeof(unsigned int);
    ulm_type[ULM_UNSIGNED].op_index = _MPI_UNSIGNED_OP_INDEX;

    ulm_type[ULM_UNSIGNED_LONG].extent = sizeof(unsigned long int);
    ulm_type[ULM_UNSIGNED_LONG].op_index = _MPI_UNSIGNED_LONG_OP_INDEX;

    ulm_type[ULM_FLOAT].extent = sizeof(float);
    ulm_type[ULM_FLOAT].op_index = _MPI_FLOAT_OP_INDEX;

    ulm_type[ULM_DOUBLE].extent = sizeof(double);
    ulm_type[ULM_DOUBLE].op_index = _MPI_DOUBLE_OP_INDEX;

    ulm_type[ULM_LONG_DOUBLE].extent = sizeof(long double);
    ulm_type[ULM_LONG_DOUBLE].op_index = _MPI_LONG_DOUBLE_OP_INDEX;

    ulm_type[ULM_PACKED].extent = sizeof(unsigned char);
    ulm_type[ULM_PACKED].op_index = _MPI_PACKED_OP_INDEX;

    ulm_type[ULM_BYTE].extent = sizeof(unsigned char);
    ulm_type[ULM_BYTE].op_index = _MPI_BYTE_OP_INDEX;

    ulm_type[ULM_LONG_LONG_INT].extent = sizeof(long long);
    ulm_type[ULM_LONG_LONG_INT].op_index = _MPI_LONG_LONG_OP_INDEX;

    ulm_type[ULM_FLOAT_INT].extent = sizeof(ulm_float_int_t);
    ulm_type[ULM_FLOAT_INT].op_index = _MPI_FLOAT_INT_OP_INDEX;

    ulm_type[ULM_DOUBLE_INT].extent = sizeof(ulm_double_int_t);
    ulm_type[ULM_DOUBLE_INT].op_index = _MPI_DOUBLE_INT_OP_INDEX;

    ulm_type[ULM_LONG_INT].extent = sizeof(ulm_long_int_t);
    ulm_type[ULM_LONG_INT].op_index = _MPI_LONG_INT_OP_INDEX;

    ulm_type[ULM_2INT].extent = sizeof(ulm_int_int_t);
    ulm_type[ULM_2INT].op_index = _MPI_2INT_OP_INDEX;

    ulm_type[ULM_SHORT_INT].extent = sizeof(ulm_short_int_t);
    ulm_type[ULM_SHORT_INT].op_index = _MPI_SHORT_INT_OP_INDEX;

    ulm_type[ULM_LONG_DOUBLE_INT].extent = sizeof(ulm_ldouble_int_t);
    ulm_type[ULM_LONG_DOUBLE_INT].op_index = _MPI_LONG_DOUBLE_INT_OP_INDEX;

    ulm_type[ULM_LB].extent = 0;
    ulm_type[ULM_LB].op_index = -1;

    ulm_type[ULM_UB].extent = 0;
    ulm_type[ULM_UB].op_index = -2;

    ulm_type[ULM_INTEGER1].extent = 1;
    ulm_type[ULM_INTEGER1].op_index = _MPI_CHAR_OP_INDEX;

    ulm_type[ULM_INTEGER2].extent = 2;
    ulm_type[ULM_INTEGER2].op_index = _MPI_SHORT_OP_INDEX;

    ulm_type[ULM_INTEGER4].extent = 4;
    ulm_type[ULM_INTEGER4].op_index = _MPI_INT_OP_INDEX;

    ulm_type[ULM_INTEGER8].extent = 8;
    ulm_type[ULM_INTEGER8].op_index = _MPI_LONG_LONG_OP_INDEX;

    ulm_type[ULM_REAL2].extent = 2;
    ulm_type[ULM_REAL2].op_index = _MPI_FLOAT_OP_INDEX;

    ulm_type[ULM_REAL4].extent = 4;
    ulm_type[ULM_REAL4].op_index = _MPI_FLOAT_OP_INDEX;

    ulm_type[ULM_REAL8].extent = 8;
    ulm_type[ULM_REAL8].op_index = _MPI_DOUBLE_OP_INDEX;

    ulm_type[ULM_COMPLEX].extent = 2 * sizeof(float);
    ulm_type[ULM_COMPLEX].op_index = _MPI_SCOMPLEX_OP_INDEX;

    ulm_type[ULM_DOUBLE_COMPLEX].extent = 2 * sizeof(double);
    ulm_type[ULM_DOUBLE_COMPLEX].op_index = _MPI_DCOMPLEX_OP_INDEX;

    ulm_type[ULM_2REAL].extent = 2 * sizeof(float);
    ulm_type[ULM_2REAL].op_index = _MPI_2REAL_OP_INDEX;

    ulm_type[ULM_2DOUBLE_PRECISION].extent = 2 * sizeof(double);
    ulm_type[ULM_2DOUBLE_PRECISION].op_index = _MPI_2DOUBLE_PRECISION_OP_INDEX;

    ulm_type[ULM_2INTEGER].extent = 2 * sizeof(int);
    ulm_type[ULM_2INTEGER].op_index = _MPI_2INT_OP_INDEX;

    /*
     * common initialization
     */

    for (i = 0; i < ULM_NUMBER_OF_TYPES ; i++) {
        tmap[i].size = ulm_type[i].extent;
        tmap[i].offset = 0;
        tmap[i].seq_offset = 0;
        ulm_type[i].type_map = &tmap[i];
        ulm_type[i].packed_size = ulm_type[i].extent;
        ulm_type[i].num_pairs = 1;
        ulm_type[i].layout = CONTIGUOUS;
        ulm_type[i].isbasic = 1;
        ulm_type[i].num_primitives = 1;
        ulm_type[i].second_primitive_offset = 0;
        ulm_type[i].committed = 1;
        ulm_type[i].ref_count = 1;
        ulm_type[i].envelope.combiner = MPI_COMBINER_NAMED;
        ulm_type[i].envelope.nints = 0;
        ulm_type[i].envelope.naddrs = 0;
        ulm_type[i].envelope.ndatatypes = 0;
    }

    /*
     * upper and lower bounds are special
     */

    ulm_type[ULM_LB].type_map = NULL;
    ulm_type[ULM_LB].num_pairs = 0;
    ulm_type[ULM_LB].num_primitives = 0;

    ulm_type[ULM_UB].type_map = NULL;
    ulm_type[ULM_UB].num_pairs = 0;
    ulm_type[ULM_UB].num_primitives = 0;

    /*
     * pair types are special
     */

    ulm_type[ULM_SHORT_INT].num_primitives = 2;
    ulm_type[ULM_2INT].num_primitives = 2;
    ulm_type[ULM_LONG_INT].num_primitives = 2;
    ulm_type[ULM_FLOAT_INT].num_primitives = 2;
    ulm_type[ULM_DOUBLE_INT].num_primitives = 2;
    ulm_type[ULM_LONG_DOUBLE_INT].num_primitives = 2;
    ulm_type[ULM_2INTEGER].num_primitives = 2;
    ulm_type[ULM_2REAL].num_primitives = 2;
    ulm_type[ULM_2DOUBLE_PRECISION].num_primitives = 2;

    ulm_type[ULM_SHORT_INT].second_primitive_offset = ulm_type[ULM_SHORT_INT].extent - sizeof(int);
    ulm_type[ULM_2INT].second_primitive_offset = ulm_type[ULM_2INT].extent - sizeof(int);
    ulm_type[ULM_LONG_INT].second_primitive_offset = ulm_type[ULM_LONG_INT].extent - sizeof(int);
    ulm_type[ULM_FLOAT_INT].second_primitive_offset = ulm_type[ULM_FLOAT_INT].extent - sizeof(int);
    ulm_type[ULM_DOUBLE_INT].second_primitive_offset = ulm_type[ULM_DOUBLE_INT].extent - sizeof(int);
    ulm_type[ULM_LONG_DOUBLE_INT].second_primitive_offset = ulm_type[ULM_LONG_DOUBLE_INT].extent - sizeof(int);
    ulm_type[ULM_2INTEGER].second_primitive_offset = ulm_type[ULM_2INTEGER].extent - sizeof(int);
    ulm_type[ULM_2REAL].second_primitive_offset = ulm_type[ULM_2REAL].extent - sizeof(float);
    ulm_type[ULM_2DOUBLE_PRECISION].second_primitive_offset = ulm_type[ULM_2DOUBLE_PRECISION].extent - sizeof(double);

    return MPI_SUCCESS;
}
