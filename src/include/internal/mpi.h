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

#ifndef _ULM_INTERNAL_MPI_H_
#define _ULM_INTERNAL_MPI_H_

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/linkage.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/no_weak_symbols.h"
#include "internal/ptr_table.h"
#include "mpi/mpi.h"
#include "ulm/ulm.h"


CDECL_BEGIN

/*
 * control
 */
enum {
    _MPI_FORTRAN = 1,
    _MPI_MARK_AS_CONTIGUOUS = 0, /* mark types as contiguous if they are */
    _MPI_DTYPE_TRIM = 0, /* interpretation of MPI standard for datatypes */
    _MPI_DEBUG = 0       /* enable debug info in MPI layer */ 
};

/*
 * typedefs
 */
typedef struct mpi_state_t mpi_state_t;
typedef struct errhandler_t errhandler_t;

/*
 * MPI state
 */
struct mpi_state_t {
    ptr_table_t *errhandler_table;
    ptr_table_t *free_table;
#if 0
    struct {
        ulm_allgather_t *allgather;
        ulm_allgatherv_t *allgatherv;
        ulm_allreduce_t *allreduce;
        ulm_alltoall_t *alltoall;
        ulm_alltoallv_t *alltoallv;
        ulm_alltoallw_t *alltoallw;
        ulm_barrier_t *barrier;
        ulm_bcast_t *bcast;
        ulm_gather_t *gather;
        ulm_gatherv_t *gatherv;
        ulm_reduce_t *reduce;
        ulm_reduce_scatter_t *reduce_scatter;
        ulm_scan_t *scan;
        ulm_scatter_t *scatter;
        ulm_scatterv_t *scatterv;
    } collective;
#endif
    MPI_Request proc_null_request;
    int fortran_layer_enabled;
    int threadsafe;
    lockStructure_t lock[1];
    int initialized;
    int finalized;
    int check_args;
    int thread_usage;
};

extern mpi_state_t _mpi;

/*********************** Data Types *************************************/

/*
 * We first define some basic structs, that we will compose into the
 * general derived mpi datatype, which consists of
 *	- A list of tuples (MPI_Datatype, offset)
 *	- A memory address
 *	- An extent (final adress - beginning address modulo alignment
 */

enum { _DATATYPE_UNDEFINED = -1 };

/*
 * Indexes for mapping basic types to pre-defined operations
 */
enum {
    _MPI_CHAR_OP_INDEX = 0,
    _MPI_SHORT_OP_INDEX,
    _MPI_INT_OP_INDEX,
    _MPI_LONG_OP_INDEX,
    _MPI_UNSIGNED_CHAR_OP_INDEX,
    _MPI_UNSIGNED_SHORT_OP_INDEX,
    _MPI_UNSIGNED_OP_INDEX,
    _MPI_UNSIGNED_LONG_OP_INDEX,
    _MPI_FLOAT_OP_INDEX,
    _MPI_DOUBLE_OP_INDEX,
    _MPI_LONG_DOUBLE_OP_INDEX,
    _MPI_PACKED_OP_INDEX,
    _MPI_BYTE_OP_INDEX,
    _MPI_LONG_LONG_OP_INDEX,
    _MPI_FLOAT_INT_OP_INDEX,
    _MPI_DOUBLE_INT_OP_INDEX,
    _MPI_LONG_INT_OP_INDEX,
    _MPI_2INT_OP_INDEX,
    _MPI_SHORT_INT_OP_INDEX,
    _MPI_LONG_DOUBLE_INT_OP_INDEX,
    _MPI_2INTEGER_OP_INDEX,
    _MPI_2REAL_OP_INDEX,
    _MPI_2DOUBLE_PRECISION_OP_INDEX,
    _MPI_SCOMPLEX_OP_INDEX,
    _MPI_DCOMPLEX_OP_INDEX,
    _MPI_QCOMPLEX_OP_INDEX,
    NUMBER_OF_BASIC_TYPES
};

/*
 * structs for basic pair types (complex and maxloc/minloc type)
 */

typedef struct scomplex_t scomplex_t;
typedef struct dcomplex_t dcomplex_t;
typedef struct pair_float_int_t pair_float_int_t;
typedef struct pair_double_int_t pair_double_int_t;
typedef struct pair_long_int_t pair_long_int_t;
typedef struct pair_short_int_t pair_short_int_t;
typedef struct pair_long_long_t pair_long_long_t;
typedef struct pair_int_int_t pair_int_int_t;
typedef struct pair_float_float_t pair_float_float_t;
typedef struct pair_double_double_t pair_double_double_t;
typedef struct pair_longdouble_longdouble_t pair_longdouble_longdouble_t;

struct scomplex_t {
    float re;
    float im;
};

struct dcomplex_t {
    double re;
    double im;
};

struct pair_float_int_t {
    float val;
    int loc;
};

struct pair_double_int_t {
    double val;
    int loc;
};

struct pair_longdouble_int_t {
    long double val;
    int loc;
};

struct pair_short_int_t {
    short val;
    int loc;
};

struct pair_int_int_t {
    int val;
    int loc;
};

struct pair_long_int_t {
    long val;
    int loc;
};

struct pair_long_long_t {
    long x;
    long y;
};

struct pair_float_float_t {
    float x;
    float y;
};

struct pair_double_double_t {
    double x;
    double y;
};

struct pair_longdouble_longdouble_t {
    long double x;
    long double y;
};

/* 
 * error handlers
 */

struct errhandler_t {
    MPI_Handler_function *func;
    lockStructure_t lock[1];
    int isbasic;
    int freed;
    int refcount;
};

/*
 * function prototypes
 */
ULMFunc_t *_mpi_get_reduction_function(MPI_Op, MPI_Datatype);
int _mpi_error(int ulm_error);
int _mpi_finalize(void);
int _mpi_init(void);
int _mpi_init_datatypes(void);
int _mpi_init_operations(void);
int _mpi_ptr_table_add(ptr_table_t *table, void *ptr);
int _mpi_ptr_table_free(ptr_table_t *table, int index);
ptr_table_t *_mpi_create_errhandler_table(void);
void *_mpi_ptr_table_lookup(ptr_table_t *table, int index);
void _mpi_dbg(const char *format, ...);
void _mpi_errhandler(MPI_Comm comm, int rc, char *file, int line);

CDECL_END

#endif /* !_ULM_INTERNAL_MPI_H_ */
