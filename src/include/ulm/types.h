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

#ifndef _ULM_TYPES_H_
#define _ULM_TYPES_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * opaque request handle
 */
typedef void *ULMRequest_t;

/*
 * Status information for a completed request
 *
 * !!!!
 * Make sure this coincides with the layout of MPI_Status in
 * mpi/types.h and is the same size as MPI_STATUS_SIZE in mpi/mpif.h
 * !!!!
 */
typedef struct ULMStatus_t ULMStatus_t;
struct ULMStatus_t {
    int peer_m;           /* peer process (source or destination) */
    int tag_m;            /* user defined tag */
    int error_m;          /* error status */
    int length_m;         /* for send, same as posted send */
    int persistent_m;     /* persistent request? */
    int state_m;          /* status of request */
};

/*
 * Structures for user-defined datatypes
 */
typedef struct ULMType_t ULMType_t;
typedef struct ULMTypeMapElt_t ULMTypeMapElt_t;
typedef struct ULMTypeEnvelope_t ULMTypeEnvelope_t;

/*
 * Enumerated type indicating whether the datatype is contiguous in
 * memory, repeated in memory (vector), or a more complex structure
 * (struct).
 */
enum ULMTypeLayout_t {
    CONTIGUOUS, NON_CONTIGUOUS
};
typedef enum ULMTypeLayout_t ULMTypeLayout_t;

/*
 * pair structure to define a typemap, a list of (datatype, offset)
 * tuples.
 *
 *   size       number of bytes at offset
 *   offset     offset, in bytes, where datatype occurs, given a base address
 *   seq_offset offset of typemap pair in 'packed' buffer
 */
struct ULMTypeMapElt_t {
    size_t size;
    ssize_t offset;
    ssize_t seq_offset;
};

/* 
 * structure representing saved datatype constructor call information -- 
 * envelope information...for MPI-2 calls: MPI_Type_get_envelope() and 
 * MPI_Type_get_contents()
 *
 * combiner         datatype constructor call
 * nints            number of integers
 * naddrs           number of addresses
 * ndatatypes       number of datatypes
 * iarray           integer array
 * aarray           address array
 * darray           datatype array
 */

struct ULMTypeEnvelope_t {
    int combiner;
    int nints;
    int naddrs;
    int ndatatypes;
    int *iarray;
    ssize_t *aarray;
    void **darray;
};

/*
 * structure representing a datatype
 *
 *   type_map       list of TypmapPairs
 *   extent         number of bytes covered by datatype,
 *                  both data and space
 *   packed_size    total amount of data in datatype
 *   num_pairs      number of pairs in typemap
 *   layout         memory layout of the datatype
 *   isbasic        flag indicating whether datatype is native mpi
 *                  or user-defined
 *   op_index       index for collective operations
 *   fhandle        the value of the fortran handle, if applicable
 *   envelope       used to record constructor information
 *   ref_count      to track the number of constructor and datatype
 *                  envelope's that reference this datatype
 *   committed      flag to determine if MPI_Type_commit has been called
 */
struct ULMType_t {
    ULMTypeMapElt_t *type_map;
    ssize_t lower_bound;
    size_t extent;
    size_t packed_size;
    int num_pairs;
    ULMTypeLayout_t layout;
    int isbasic;
    int num_primitives;
    int second_primitive_offset;
    int op_index;
    int fhandle;
    ULMTypeEnvelope_t envelope;
    int ref_count;
    int committed;
};

/*
 * structure for basic and user-defined operations
 *
 *   func       array of function pointers
 *   isbasic    true if this is a pre-defined operation
 *   commute    true if this is a commutative function
 *   fortran    true if this is a fortran operation
 *   fhandle    the value of the fortran handle, if applicable
 */
typedef void (ULMFunc_t) (void *, void *, int *, void *);
typedef struct ULMOp_t ULMOp_t;
struct ULMOp_t {
    ULMFunc_t **func;
    int isbasic;
    int commute;
    int fortran;
    int fhandle;
};

/*
 * struct containing host information for ulm_utsend
 */
typedef struct ULMMcastInfo_t ULMMcastInfo_t;
struct ULMMcastInfo_t {
    int hostID;			/* host ID number within group */
    int nGroupProcIDOnHost;	/* count of how many to send to  */
};


/*
 * types for complex
 */

typedef struct ulm_scomplex_t ulm_scomplex_t;
typedef struct ulm_dcomplex_t ulm_dcomplex_t;
typedef struct ulm_qcomplex_t ulm_qcomplex_t;

struct ulm_scomplex_t {
    float re;
    float im;
};

struct ulm_dcomplex_t {
    double re;
    double im;
};

struct ulm_qcomplex_t {
    long double re;
    long double im;
};

/*
 * types for maxloc/minloc reduction operations
 */

typedef struct ulm_float_int_t ulm_float_int_t;
typedef struct ulm_double_int_t ulm_double_int_t;
typedef struct ulm_ldouble_int_t ulm_ldouble_int_t;
typedef struct ulm_char_int_t ulm_char_int_t;
typedef struct ulm_short_int_t ulm_short_int_t;
typedef struct ulm_int_int_t ulm_int_int_t;
typedef struct ulm_long_int_t ulm_long_int_t;
typedef struct ulm_uchar_int_t ulm_uchar_int_t;
typedef struct ulm_ushort_int_t ulm_ushort_int_t;
typedef struct ulm_uint_int_t ulm_uint_int_t;
typedef struct ulm_ulong_int_t ulm_ulong_int_t;

typedef struct ulm_float_float_t ulm_float_float_t;
typedef struct ulm_double_double_t ulm_double_double_t;
typedef struct ulm_ldouble_ldouble_t ulm_ldouble_ldouble_t;

struct ulm_float_int_t {
    float val;
    int loc;
};

struct ulm_double_int_t {
    double val;
    int loc;
};

struct ulm_ldouble_int_t {
    long double val;
    int loc;
};

struct ulm_char_int_t {
    char val;
    int loc;
};

struct ulm_short_int_t {
    short val;
    int loc;
};

struct ulm_int_int_t {
    int val;
    int loc;
};

struct ulm_long_int_t {
    long val;
    int loc;
};

struct ulm_uchar_int_t {
    unsigned char val;
    int loc;
};

struct ulm_ushort_int_t {
    unsigned short val;
    int loc;
};

struct ulm_uint_int_t {
    unsigned int val;
    int loc;
};

struct ulm_ulong_int_t {
    unsigned long val;
    int loc;
};

struct ulm_float_float_t {
    float val;
    float loc;
};

struct ulm_double_double_t {
    double val;
    double loc;
};

struct ulm_ldouble_ldouble_t {
    long double val;
    long double loc;
};


/*
 * Union for internal description of topologies
 */

typedef union ULMTopology_t ULMTopology_t;
union ULMTopology_t {
    int type;
    struct {
	int type;
	int nnodes;
	int ndims;
	int *dims;
	int *periods;
	int *position;
    } cart;
    struct {
	int type;
	int nnodes;
	int nedges;
	int *index;
	int *edges;
    } graph;
};


/*
 * Collective function types
 */

typedef int (ulm_allgather_t) (void *, int, ULMType_t *, void *, int,
			       ULMType_t *, int);
typedef int (ulm_allgatherv_t) (void *, int, ULMType_t *, void *, int *,
				int *, ULMType_t *, int);
typedef int (ulm_allreduce_t) (const void *, void *, int, ULMType_t *,
			       ULMOp_t *, int);
typedef int (ulm_alltoall_t) (void *, int, ULMType_t *, void *, int,
			      ULMType_t *, int);
typedef int (ulm_alltoallv_t) (void *, int *, int *, ULMType_t *, void *,
			       int *, int *, ULMType_t *, int);
typedef int (ulm_alltoallw_t) (void *, int *, int *, ULMType_t **, void *,
			       int *, int *, ULMType_t **, int);
typedef int (ulm_barrier_t) (int);
typedef int (ulm_bcast_t) (void *, int, ULMType_t *, int, int);
typedef int (ulm_gather_t) (void *, int, ULMType_t *, void *, int,
			    ULMType_t *, int, int);
typedef int (ulm_gatherv_t) (void *, int, ULMType_t *, void *, int *,
			     int *, ULMType_t *, int, int);
typedef int (ulm_reduce_t) (const void *, void *, int, ULMType_t *,
			    ULMOp_t *, int, int);
typedef int (ulm_reduce_scatter_t) (void *, void *, int *, ULMType_t *,
				    ULMOp_t *, int);
typedef int (ulm_scan_t) (const void *, void *, int, ULMType_t *,
			  ULMOp_t *, int);
typedef int (ulm_scatter_t) (void *, int, ULMType_t *, void *, int,
			     ULMType_t *, int, int);
typedef int (ulm_scatterv_t) (void *, int *, int *, ULMType_t *, void *,
			      int, ULMType_t *, int, int);

/*
 * datatype reference-count-based retain/release functions
 */

#include "os/atomic.h"

/*
 * datatype retain: increment reference count
 */
#define ulm_type_retain(type)             \
do {                                      \
    if (type && type->isbasic == 0) {     \
        fetchNadd(&(type->ref_count), 1); \
    }                                     \
} while (0)

/*
 * datatype release: decrement reference count and free resources if
 * the count reaches 0
 */
#define ulm_type_release(type)                        \
do {                                                  \
    if (type && !type->isbasic) {                     \
        if (fetchNadd(&(type->ref_count), -1) == 1) { \
            ulm_type_free(type);                      \
        }                                             \
    }                                                 \
} while (0)


#ifdef __cplusplus
}
#endif

#endif /* _ULM_TYPES_H_ */
