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

#include <string.h>

#include "internal/mpi.h"

/*
 * External symbols for basic datatypes
 */
MPI_Datatype MPI_CHAR;
MPI_Datatype MPI_SHORT;
MPI_Datatype MPI_INT;
MPI_Datatype MPI_LONG;
MPI_Datatype MPI_UNSIGNED_CHAR;
MPI_Datatype MPI_UNSIGNED_SHORT;
MPI_Datatype MPI_UNSIGNED;
MPI_Datatype MPI_UNSIGNED_LONG;
MPI_Datatype MPI_FLOAT;
MPI_Datatype MPI_DOUBLE;
MPI_Datatype MPI_LONG_DOUBLE;
MPI_Datatype MPI_PACKED;
MPI_Datatype MPI_BYTE;
MPI_Datatype MPI_LONG_LONG_INT;
MPI_Datatype MPI_FLOAT_INT;
MPI_Datatype MPI_DOUBLE_INT;
MPI_Datatype MPI_LONG_INT;
MPI_Datatype MPI_2INT;
MPI_Datatype MPI_SHORT_INT;
MPI_Datatype MPI_LONG_DOUBLE_INT;
MPI_Datatype MPI_LB;
MPI_Datatype MPI_UB;

/*
 * Basic datatype initialization
 */
int _mpi_init_datatypes(void)
{
    ULMType_t *type;
    ULMTypeMapElt_t *pair;
    int i;

    enum {
        I_CHAR = 0,
        I_SHORT,
        I_INT,
        I_LONG,
        I_UNSIGNED_CHAR,
        I_UNSIGNED_SHORT,
        I_UNSIGNED,
        I_UNSIGNED_LONG,
        I_FLOAT,
        I_DOUBLE,
        I_LONG_DOUBLE,
        I_PACKED,
        I_BYTE,
        I_LONG_LONG_INT,
        I_FLOAT_INT,
        I_DOUBLE_INT,
        I_LONG_INT,
        I_2INT,
        I_SHORT_INT,
        I_LONG_DOUBLE_INT,
        I_LB,
        I_UB,
        NUMBER_OF_TYPES
    };

    type = ulm_malloc(NUMBER_OF_TYPES * sizeof(ULMType_t));
    if (type == NULL) {
        return -1;
    }
    memset(type, 0, NUMBER_OF_TYPES * sizeof(ULMType_t));

    pair = ulm_malloc(NUMBER_OF_TYPES * sizeof(ULMTypeMapElt_t));
    if (pair == NULL) {
        free(type);
        return -1;
    }
    memset(pair, 0, NUMBER_OF_TYPES * sizeof(ULMTypeMapElt_t));

    /*
     * specific initialization
     */

    type[I_CHAR].extent = sizeof(signed char);
    type[I_CHAR].op_index = _MPI_CHAR_OP_INDEX;

    type[I_SHORT].extent = sizeof(signed short int);
    type[I_SHORT].op_index = _MPI_SHORT_OP_INDEX;

    type[I_INT].extent = sizeof(signed int);
    type[I_INT].op_index = _MPI_INT_OP_INDEX;

    type[I_LONG].extent = sizeof(signed long int);
    type[I_LONG].op_index = _MPI_LONG_OP_INDEX;

    type[I_UNSIGNED_CHAR].extent = sizeof(unsigned char);
    type[I_UNSIGNED_CHAR].op_index = _MPI_UNSIGNED_CHAR_OP_INDEX;

    type[I_UNSIGNED_SHORT].extent = sizeof(unsigned short int);
    type[I_UNSIGNED_SHORT].op_index = _MPI_UNSIGNED_SHORT_OP_INDEX;

    type[I_UNSIGNED].extent = sizeof(unsigned int);
    type[I_UNSIGNED].op_index = _MPI_UNSIGNED_OP_INDEX;

    type[I_UNSIGNED_LONG].extent = sizeof(unsigned long int);
    type[I_UNSIGNED_LONG].op_index = _MPI_UNSIGNED_LONG_OP_INDEX;

    type[I_FLOAT].extent = sizeof(float);
    type[I_FLOAT].op_index = _MPI_FLOAT_OP_INDEX;

    type[I_DOUBLE].extent = sizeof(double);
    type[I_DOUBLE].op_index = _MPI_DOUBLE_OP_INDEX;

    type[I_LONG_DOUBLE].extent = sizeof(long double);
    type[I_LONG_DOUBLE].op_index = _MPI_LONG_DOUBLE_OP_INDEX;

    type[I_PACKED].extent = sizeof(unsigned char);
    type[I_PACKED].op_index = _MPI_PACKED_OP_INDEX;

    type[I_BYTE].extent = sizeof(unsigned char);
    type[I_BYTE].op_index = _MPI_BYTE_OP_INDEX;

    type[I_LONG_LONG_INT].extent = sizeof(long long);
    type[I_LONG_LONG_INT].op_index = _MPI_LONG_LONG_OP_INDEX;

    type[I_FLOAT_INT].extent = sizeof(ulm_float_int_t);
    type[I_FLOAT_INT].op_index = _MPI_FLOAT_INT_OP_INDEX;

    type[I_DOUBLE_INT].extent = sizeof(ulm_double_int_t);
    type[I_DOUBLE_INT].op_index = _MPI_DOUBLE_INT_OP_INDEX;

    type[I_LONG_INT].extent = sizeof(ulm_long_int_t);
    type[I_LONG_INT].op_index = _MPI_LONG_INT_OP_INDEX;

    type[I_2INT].extent = sizeof(ulm_int_int_t);
    type[I_2INT].op_index = _MPI_2INT_OP_INDEX;

    type[I_SHORT_INT].extent = sizeof(ulm_short_int_t);
    type[I_SHORT_INT].op_index = _MPI_SHORT_INT_OP_INDEX;

    type[I_LONG_DOUBLE_INT].extent = sizeof(ulm_ldouble_int_t);
    type[I_LONG_DOUBLE_INT].op_index = _MPI_LONG_DOUBLE_INT_OP_INDEX;

    type[I_LB].extent = 0;
    type[I_LB].op_index = -1;

    type[I_UB].extent = 0;
    type[I_UB].op_index = -2;

    /*
     * common initialization
     */

    for (i = 0; i < NUMBER_OF_TYPES; i++) {
        pair[i].size = type[i].extent;
        pair[i].offset = 0;
        pair[i].seq_offset = 0;
        type[i].type_map = &pair[i];
        type[i].packed_size = type[i].extent;
        type[i].num_pairs = 1;
        type[i].layout = CONTIGUOUS;
        type[i].isbasic = 1;
        type[i].committed = 1;
        type[i].ref_count = 1;
        type[i].envelope.combiner = MPI_COMBINER_NAMED;
        type[i].envelope.nints = 0;
        type[i].envelope.naddrs = 0;
        type[i].envelope.ndatatypes = 0;
    }

    /*
     * upper and lower bounds are special
     */

    type[I_LB].type_map = NULL;
    type[I_LB].num_pairs = 0;

    type[I_UB].type_map = NULL;
    type[I_UB].num_pairs = 0;

    /*
     * assign external symbols
     */

    MPI_CHAR = &(type[I_CHAR]);
    MPI_SHORT = &(type[I_SHORT]);
    MPI_INT = &(type[I_INT]);
    MPI_LONG = &(type[I_LONG]);
    MPI_UNSIGNED_CHAR = &(type[I_UNSIGNED_CHAR]);
    MPI_UNSIGNED_SHORT = &(type[I_UNSIGNED_SHORT]);
    MPI_UNSIGNED = &(type[I_UNSIGNED]);
    MPI_UNSIGNED_LONG = &(type[I_UNSIGNED_LONG]);
    MPI_FLOAT = &(type[I_FLOAT]);
    MPI_DOUBLE = &(type[I_DOUBLE]);
    MPI_LONG_DOUBLE = &(type[I_LONG_DOUBLE]);
    MPI_PACKED = &(type[I_PACKED]);
    MPI_BYTE = &(type[I_BYTE]);
    MPI_LONG_LONG_INT = &(type[I_LONG_LONG_INT]);
    MPI_FLOAT_INT = &(type[I_FLOAT_INT]);
    MPI_DOUBLE_INT = &(type[I_DOUBLE_INT]);
    MPI_LONG_INT = &(type[I_LONG_INT]);
    MPI_2INT = &(type[I_2INT]);
    MPI_SHORT_INT = &(type[I_SHORT_INT]);
    MPI_LONG_DOUBLE_INT = &(type[I_LONG_DOUBLE_INT]);
    MPI_LB = &(type[I_LB]);
    MPI_UB = &(type[I_UB]);

    return MPI_SUCCESS;
}
