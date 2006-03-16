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



#ifndef _ULM_MPI_CONSTANTS_H_
#define _ULM_MPI_CONSTANTS_H_

#include "ulm/constants.h"
#include "mpi/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************** Constants **************************************/

/*
 * Null objects
 */
extern void *ulm_datatype_null_location;
extern void *ulm_op_null_location;
extern void *ulm_request_null_location;

#define MPI_BOTTOM              ((MPI_Aint) NULL)
#define MPI_COMM_NULL           ((MPI_Comm) -1)
#define MPI_GROUP_NULL          ((MPI_Group) -1)
#define MPI_GROUP_EMPTY         ((MPI_Group) -2)
#define MPI_DATATYPE_NULL       ((MPI_Datatype) &ulm_datatype_null_location)
#define MPI_OP_NULL             ((MPI_Op) &ulm_op_null_location)
#define MPI_REQUEST_NULL        ((MPI_Request) &ulm_request_null_location)
#define MPI_ERRHANDLER_NULL     ((MPI_Errhandler) -1)
#define MPI_STATUS_IGNORE       ((MPI_Status *) NULL)
#define MPI_STATUSES_IGNORE     ((MPI_Status *) NULL)
#define MPI_IN_PLACE            ((void *) -1)

/*
 * Pointer value to indicate that send buffer should be used for
 */


enum {

    /*
     * Version
     */
    MPI_VERSION = 1,
    MPI_SUBVERSION = 2,

    /*
     * Communicators
     */
    MPI_COMM_WORLD = ULM_COMM_WORLD,
    MPI_COMM_SELF = ULM_COMM_SELF,
    MPI_COMM_PARENT = -1,
    MPI_COMM_INTERCOMM = -1,

    /*
     * Limits
     */
    MPI_MAX_PROCESSOR_NAME = 256,
    MPI_MAX_ERROR_STRING = 256,
    MPI_MAX_NAME_STRING = 256,
    MPI_BSEND_OVERHEAD = 0,

    /*
     * Wildcards etc.
     */
    MPI_ANY_SOURCE = ULM_ANY_PROC,
    MPI_ANY_TAG = ULM_ANY_TAG,
    MPI_PROC_NULL = -2,
    MPI_ROOT = -3,
    MPI_UNDEFINED = -1,

    /*
     * Group comparisons
     */
    MPI_IDENT = 0,
    MPI_CONGRUENT = 1,
    MPI_SIMILAR = 2,
    MPI_UNEQUAL = 3,

    /*
     * Attribute caching
     */
    MPI_KEYVAL_INVALID = -1,

    /*
     * Environmental inquiry keys (C and Fortran)
     */
    MPI_TAG_UB = -2,
    MPI_IO = -3,
    MPI_HOST = -4,
    MPI_WTIME_IS_GLOBAL = -5,
    MPI_TAG_UB_VALUE = 0x7fffffff,

    /*
     * Topology types
     */
    MPI_CART = 1,
    MPI_GRAPH = 2,

    /*
     * Default error handlers
     */
    MPI_ERRORS_ARE_FATAL = 0,
    MPI_ERRORS_RETURN = 1,
    MPI_ERRHANDLER_DEFAULT = MPI_ERRORS_RETURN,

    /*
     * MPI-2 combiner values
     */
    MPI_COMBINER_NAMED = 0,
    MPI_COMBINER_DUP = 1,
    MPI_COMBINER_CONTIGUOUS = 2,
    MPI_COMBINER_VECTOR = 3,
    MPI_COMBINER_HVECTOR_INTEGER = 4,
    MPI_COMBINER_HVECTOR = 5,
    MPI_COMBINER_INDEXED = 6,
    MPI_COMBINER_HINDEXED_INTEGER = 7,
    MPI_COMBINER_HINDEXED = 8,
    MPI_COMBINER_INDEXED_BLOCK = 9,
    MPI_COMBINER_STRUCT_INTEGER = 10,
    MPI_COMBINER_STRUCT = 11,
    MPI_COMBINER_SUBARRAY = 12,
    MPI_COMBINER_DARRAY = 13,
    MPI_COMBINER_F90_REAL = 14,
    MPI_COMBINER_F90_COMPLEX = 15,
    MPI_COMBINER_F90_INTEGER = 16,
    MPI_COMBINER_RESIZED = 17,

    /* WARNING WARNING WARNING - all MPI error codes must be
     *  greater than zero, and all ULM error codes must be less
     *  than zero for _mpi_error() to work correctly! MPI_SUCCESS
     *  equals ULM_SUCCESS which equals 0!
     */

    /*
     * Return codes: MPI 1
     */
    MPI_SUCCESS = 0,
    MPI_ERR_BUFFER = 1,
    MPI_ERR_COUNT = 2,
    MPI_ERR_TYPE = 3,
    MPI_ERR_TAG = 4,
    MPI_ERR_COMM = 5,
    MPI_ERR_RANK = 6,
    MPI_ERR_REQUEST = 7,
    MPI_ERR_ROOT = 8,
    MPI_ERR_GROUP = 9,
    MPI_ERR_OP = 10,
    MPI_ERR_TOPOLOGY = 11,
    MPI_ERR_DIMS = 12,
    MPI_ERR_ARG = 13,
    MPI_ERR_UNKNOWN = 14,
    MPI_ERR_TRUNCATE = 15,
    MPI_ERR_OTHER = 16,
    MPI_ERR_INTERN = 17,
    MPI_ERR_PENDING = 18,
    MPI_ERR_IN_STATUS = 19,

    /*
     * Return codes MPI 2
     */
    MPI_ERR_ACCESS = 21,
    MPI_ERR_AMODE = 22,
    MPI_ERR_ASSERT = 23,
    MPI_ERR_BAD_FILE = 24,
    MPI_ERR_BASE = 25,
    MPI_ERR_CONVERSION = 26,
    MPI_ERR_DISP = 27,
    MPI_ERR_DUP_DATAREP = 28,
    MPI_ERR_FILE_EXISTS = 29,
    MPI_ERR_FILE_IN_USE = 30,
    MPI_ERR_FILE = 31,
    MPI_ERR_INFO_KEY = 32,
    MPI_ERR_INFO_NOKEY = 33,
    MPI_ERR_INFO_VALUE = 34,
    MPI_ERR_INFO = 35,
    MPI_ERR_IO = 36,
    MPI_ERR_KEYVAL = 37,
    MPI_ERR_LOCKTYPE = 38,
    MPI_ERR_NAME = 39,
    MPI_ERR_NO_MEM = 40,
    MPI_ERR_NOT_SAME = 41,
    MPI_ERR_NO_SPACE = 42,
    MPI_ERR_NO_SUCH_FILE = 43,
    MPI_ERR_PORT = 44,
    MPI_ERR_QUOTA = 45,
    MPI_ERR_READ_ONLY = 46,
    MPI_ERR_RMA_CONFLICT = 47,
    MPI_ERR_RMA_SYNC = 48,
    MPI_ERR_SERVICE = 49,
    MPI_ERR_SIZE = 50,
    MPI_ERR_SPAWN = 51,
    MPI_ERR_UNSUPPORTED_DATAREP = 52,
    MPI_ERR_UNSUPPORTED_OPERATION = 53,
    MPI_ERR_WIN = 54,

    MPI_ERR_LASTCODE = 55,

    /* mpi-2 thread constants */
    MPI_THREAD_SINGLE=1,
    MPI_THREAD_FUNNELED,
    MPI_THREAD_SERIALIZED,
    MPI_THREAD_MULTIPLE

};
/* attribute function macros */

#define MPI_DUP_FN              mpi_dup_fn_c
#define MPI_NULL_COPY_FN        mpi_null_copy_fn_c
#define MPI_NULL_DELETE_FN      mpi_null_delete_fn_c

#ifdef __cplusplus
}
#endif

#endif /* !_ULM_MPI_CONSTANTS_H_ */
