!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!
! Copyright 2002-2003. The Regents of the University of California. This material 
! was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
! National Laboratory, which is operated by the University of California for 
! the U.S. Department of Energy. The Government is granted for itself and 
! others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
! license in this material to reproduce, prepare derivative works, and 
! perform publicly and display publicly. Beginning five (5) years after 
! October 10,2002 subject to additional five-year worldwide renewals, the 
! Government is granted for itself and others acting on its behalf a paid-up, 
! nonexclusive, irrevocable worldwide license in this material to reproduce, 
! prepare derivative works, distribute copies to the public, perform publicly 
! and display publicly, and to permit others to do so. NEITHER THE UNITED 
! STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
! CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
! IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
! COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
! PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
! OWNED RIGHTS.

! Additionally, this program is free software; you can distribute it and/or 
! modify it under the terms of the GNU Lesser General Public License as 
! published by the Free Software Foundation; either version 2 of the License, 
! or any later version.  Accordingly, this program is distributed in the hope 
! that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
! warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
! GNU Lesser General Public License for more details.
!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

!     function types

      double precision PMPI_WTIME, PMPI_WTICK
      external PMPI_WTIME, PMPI_WTICK
      double precision MPI_WTIME, MPI_WTICK
      external MPI_WTIME, MPI_WTICK

!     status size and reserved index values (Fortran)

      INTEGER MPI_STATUS_SIZE
      INTEGER MPI_SOURCE
      INTEGER MPI_TAG
      INTEGER MPI_ERROR

      PARAMETER(MPI_STATUS_SIZE = 5)
      PARAMETER(MPI_SOURCE = 1)
      PARAMETER(MPI_TAG = 2)
      PARAMETER(MPI_ERROR = 3)

!     basic datatypes (Fortran)

      INTEGER MPI_INTEGER
      INTEGER MPI_REAL
      INTEGER MPI_DOUBLE_PRECISION
      INTEGER MPI_COMPLEX
      INTEGER MPI_DOUBLE_COMPLEX
      INTEGER MPI_LOGICAL
      INTEGER MPI_CHARACTER
      INTEGER MPI_BYTE
      INTEGER MPI_PACKED
      INTEGER MPI_2REAL
      INTEGER MPI_2DOUBLE_PRECISION
      INTEGER MPI_2INTEGER
      INTEGER MPI_INTEGER1
      INTEGER MPI_INTEGER2
      INTEGER MPI_INTEGER4
      INTEGER MPI_REAL2
      INTEGER MPI_REAL4
      INTEGER MPI_REAL8
      INTEGER MPI_LB
      INTEGER MPI_UB

      PARAMETER(MPI_INTEGER = 0)
      PARAMETER(MPI_REAL = 1)
      PARAMETER(MPI_DOUBLE_PRECISION = 2)
      PARAMETER(MPI_COMPLEX = 3)
      PARAMETER(MPI_DOUBLE_COMPLEX = 4)
      PARAMETER(MPI_LOGICAL = 5)
      PARAMETER(MPI_CHARACTER = 6)
      PARAMETER(MPI_BYTE = 7)
      PARAMETER(MPI_PACKED = 8)
      PARAMETER(MPI_2REAL = 9)
      PARAMETER(MPI_2DOUBLE_PRECISION = 10)
      PARAMETER(MPI_2INTEGER = 11)
      PARAMETER(MPI_INTEGER1 = 12)
      PARAMETER(MPI_INTEGER2 = 13)
      PARAMETER(MPI_INTEGER4 = 14)
      PARAMETER(MPI_REAL2 = 15)
      PARAMETER(MPI_REAL4 = 16)
      PARAMETER(MPI_REAL8 = 17)
      PARAMETER(MPI_LB = 18)
      PARAMETER(MPI_UB = 19)

!     collective operations (C and Fortran)

      INTEGER MPI_MAX
      INTEGER MPI_MIN
      INTEGER MPI_SUM
      INTEGER MPI_PROD
      INTEGER MPI_MAXLOC
      INTEGER MPI_MINLOC
      INTEGER MPI_BAND
      INTEGER MPI_BOR
      INTEGER MPI_BXOR
      INTEGER MPI_LAND
      INTEGER MPI_LOR
      INTEGER MPI_LXOR

      PARAMETER(MPI_MAX = 0)
      PARAMETER(MPI_MIN = 1)
      PARAMETER(MPI_SUM = 2)
      PARAMETER(MPI_PROD = 3)
      PARAMETER(MPI_MAXLOC = 4)
      PARAMETER(MPI_MINLOC = 5)
      PARAMETER(MPI_BAND = 6)
      PARAMETER(MPI_BOR = 7)
      PARAMETER(MPI_BXOR = 8)
      PARAMETER(MPI_LAND = 9)
      PARAMETER(MPI_LOR = 10)
      PARAMETER(MPI_LXOR = 11)

!     special values

      INTEGER MPI_ANY_SOURCE
      INTEGER MPI_ANY_TAG
      INTEGER MPI_BOTTOM
      INTEGER MPI_BSEND_OVERHEAD
      INTEGER MPI_CART
      INTEGER MPI_COMM_NULL
      INTEGER MPI_COMM_SELF
      INTEGER MPI_COMM_WORLD
      INTEGER MPI_CONGRUENT
      INTEGER MPI_DATATYPE_NULL
      INTEGER MPI_ERRHANDLER_NULL
      INTEGER MPI_ERRORS_ARE_FATAL
      INTEGER MPI_ERRORS_RETURN
      INTEGER MPI_GRAPH
      INTEGER MPI_GROUP_EMPTY
      INTEGER MPI_GROUP_NULL
      INTEGER MPI_IDENT
      INTEGER MPI_IN_PLACE
      INTEGER MPI_KEYVAL_INVALID
      INTEGER MPI_MAX_ERROR_STRING
      INTEGER MPI_MAX_NAME_STRING
      INTEGER MPI_MAX_PROCESSOR_NAME
      INTEGER MPI_OP_NULL
      INTEGER MPI_PROC_NULL
      INTEGER MPI_REQUEST_NULL
      INTEGER MPI_SIMILAR
      INTEGER MPI_TAG_UB
      INTEGER MPI_IO
      INTEGER MPI_HOST
      INTEGER MPI_WTIME_IS_GLOBAL
      INTEGER MPI_UNDEFINED
      INTEGER MPI_UNEQUAL

      PARAMETER(MPI_ANY_SOURCE = -1)
      PARAMETER(MPI_ANY_TAG = -1)
      PARAMETER(MPI_BOTTOM = 0)
      PARAMETER(MPI_BSEND_OVERHEAD = 0)
      PARAMETER(MPI_CART = 1)
      PARAMETER(MPI_COMM_NULL = -1)
      PARAMETER(MPI_COMM_SELF = 2)
      PARAMETER(MPI_COMM_WORLD = 1)
      PARAMETER(MPI_CONGRUENT = 1)
      PARAMETER(MPI_DATATYPE_NULL = -1)
      PARAMETER(MPI_ERRHANDLER_NULL = -1)
      PARAMETER(MPI_ERRORS_ARE_FATAL = 0)
      PARAMETER(MPI_ERRORS_RETURN = 1)
      PARAMETER(MPI_GRAPH = 2)
      PARAMETER(MPI_GROUP_EMPTY = -2)
      PARAMETER(MPI_GROUP_NULL = -1)
      PARAMETER(MPI_IDENT = 0)
      PARAMETER(MPI_IN_PLACE = -1)
      PARAMETER(MPI_KEYVAL_INVALID = -1)
      PARAMETER(MPI_MAX_ERROR_STRING = 256)
      PARAMETER(MPI_MAX_NAME_STRING = 256)
      PARAMETER(MPI_MAX_PROCESSOR_NAME = 256)
      PARAMETER(MPI_OP_NULL = -1)
      PARAMETER(MPI_PROC_NULL = -2 )
      PARAMETER(MPI_REQUEST_NULL = -1)
      PARAMETER(MPI_SIMILAR = 2)
      PARAMETER(MPI_TAG_UB = -2)
      PARAMETER(MPI_IO = -3)
      PARAMETER(MPI_HOST = -4)
      PARAMETER(MPI_WTIME_IS_GLOBAL = -5)
      PARAMETER(MPI_UNDEFINED = -1)
      PARAMETER(MPI_UNEQUAL = 3)

!     MPI-2 combiner values

      INTEGER MPI_COMBINER_NAMED
      INTEGER MPI_COMBINER_DUP
      INTEGER MPI_COMBINER_CONTIGUOUS
      INTEGER MPI_COMBINER_VECTOR
      INTEGER MPI_COMBINER_HVECTOR_INTEGER
      INTEGER MPI_COMBINER_HVECTOR
      INTEGER MPI_COMBINER_INDEXED
      INTEGER MPI_COMBINER_HINDEXED_INTEGER
      INTEGER MPI_COMBINER_HINDEXED
      INTEGER MPI_COMBINER_INDEXED_BLOCK
      INTEGER MPI_COMBINER_STRUCT_INTEGER
      INTEGER MPI_COMBINER_STRUCT
      INTEGER MPI_COMBINER_SUBARRAY
      INTEGER MPI_COMBINER_DARRAY
      INTEGER MPI_COMBINER_F90_REAL
      INTEGER MPI_COMBINER_F90_COMPLEX
      INTEGER MPI_COMBINER_F90_INTEGER
      INTEGER MPI_COMBINER_RESIZED

      PARAMETER(MPI_COMBINER_NAMED = 0)
      PARAMETER(MPI_COMBINER_DUP = 1)
      PARAMETER(MPI_COMBINER_CONTIGUOUS = 2)
      PARAMETER(MPI_COMBINER_VECTOR = 3)
      PARAMETER(MPI_COMBINER_HVECTOR_INTEGER = 4)
      PARAMETER(MPI_COMBINER_HVECTOR = 5)
      PARAMETER(MPI_COMBINER_INDEXED = 6)
      PARAMETER(MPI_COMBINER_HINDEXED_INTEGER = 7)
      PARAMETER(MPI_COMBINER_HINDEXED = 8)
      PARAMETER(MPI_COMBINER_INDEXED_BLOCK = 9)
      PARAMETER(MPI_COMBINER_STRUCT_INTEGER = 10)
      PARAMETER(MPI_COMBINER_STRUCT = 11)
      PARAMETER(MPI_COMBINER_SUBARRAY = 12)
      PARAMETER(MPI_COMBINER_DARRAY = 13)
      PARAMETER(MPI_COMBINER_F90_REAL = 14)
      PARAMETER(MPI_COMBINER_F90_COMPLEX = 15)
      PARAMETER(MPI_COMBINER_F90_INTEGER = 16)
      PARAMETER(MPI_COMBINER_RESIZED = 17)
    
!     MPI error codes

      INTEGER MPI_SUCCESS
      INTEGER MPI_ERR_BUFFER
      INTEGER MPI_ERR_COUNT
      INTEGER MPI_ERR_TYPE
      INTEGER MPI_ERR_TAG
      INTEGER MPI_ERR_COMM
      INTEGER MPI_ERR_RANK
      INTEGER MPI_ERR_REQUEST
      INTEGER MPI_ERR_ROOT
      INTEGER MPI_ERR_GROUP
      INTEGER MPI_ERR_OP
      INTEGER MPI_ERR_TOPOLOGY
      INTEGER MPI_ERR_DIMS
      INTEGER MPI_ERR_ARG
      INTEGER MPI_ERR_UNKNOWN
      INTEGER MPI_ERR_TRUNCATE
      INTEGER MPI_ERR_OTHER
      INTEGER MPI_ERR_INTERN
      INTEGER MPI_ERR_PENDING
      INTEGER MPI_ERR_IN_STATUS

!     MPI 2 error codes

      INTEGER MPI_ERR_ACCESS
      INTEGER MPI_ERR_AMODE
      INTEGER MPI_ERR_ASSERT
      INTEGER MPI_ERR_BAD_FILE
      INTEGER MPI_ERR_BASE
      INTEGER MPI_ERR_CONVERSION
      INTEGER MPI_ERR_DISP
      INTEGER MPI_ERR_DUP_DATAREP
      INTEGER MPI_ERR_FILE_EXISTS
      INTEGER MPI_ERR_FILE_IN_USE
      INTEGER MPI_ERR_FILE
      INTEGER MPI_ERR_INFO_KEY
      INTEGER MPI_ERR_INFO_NOKEY
      INTEGER MPI_ERR_INFO_VALUE
      INTEGER MPI_ERR_INFO
      INTEGER MPI_ERR_IO
      INTEGER MPI_ERR_KEYVAL
      INTEGER MPI_ERR_LOCKTYPE
      INTEGER MPI_ERR_NAME
      INTEGER MPI_ERR_NO_MEM
      INTEGER MPI_ERR_NOT_SAME
      INTEGER MPI_ERR_NO_SPACE
      INTEGER MPI_ERR_NO_SUCH_FILE
      INTEGER MPI_ERR_PORT
      INTEGER MPI_ERR_QUOTA
      INTEGER MPI_ERR_READ_ONLY
      INTEGER MPI_ERR_RMA_CONFLICT
      INTEGER MPI_ERR_RMA_SYNC
      INTEGER MPI_ERR_SERVICE
      INTEGER MPI_ERR_SIZE
      INTEGER MPI_ERR_SPAWN
      INTEGER MPI_ERR_UNSUPPORTED_DATAREP
      INTEGER MPI_ERR_UNSUPPORTED_OPERATION
      INTEGER MPI_ERR_WIN

      INTEGER MPI_ERR_LASTCODE

      PARAMETER(MPI_SUCCESS = 0)
      PARAMETER(MPI_ERR_BUFFER = 1)
      PARAMETER(MPI_ERR_COUNT = 2)
      PARAMETER(MPI_ERR_TYPE = 3)
      PARAMETER(MPI_ERR_TAG = 4)
      PARAMETER(MPI_ERR_COMM = 5)
      PARAMETER(MPI_ERR_RANK = 6)
      PARAMETER(MPI_ERR_REQUEST = 7)
      PARAMETER(MPI_ERR_ROOT = 8)
      PARAMETER(MPI_ERR_GROUP = 9)
      PARAMETER(MPI_ERR_OP = 10)
      PARAMETER(MPI_ERR_TOPOLOGY = 11)
      PARAMETER(MPI_ERR_DIMS = 12)
      PARAMETER(MPI_ERR_ARG = 13)
      PARAMETER(MPI_ERR_UNKNOWN = 14)
      PARAMETER(MPI_ERR_TRUNCATE = 15)
      PARAMETER(MPI_ERR_OTHER = 16)
      PARAMETER(MPI_ERR_INTERN = 17)
      PARAMETER(MPI_ERR_PENDING = 18)
      PARAMETER(MPI_ERR_IN_STATUS = 19)

      PARAMETER(MPI_ERR_ACCESS = 21)
      PARAMETER(MPI_ERR_AMODE = 22)
      PARAMETER(MPI_ERR_ASSERT = 23)
      PARAMETER(MPI_ERR_BAD_FILE = 24)
      PARAMETER(MPI_ERR_BASE = 25)
      PARAMETER(MPI_ERR_CONVERSION = 26)
      PARAMETER(MPI_ERR_DISP = 27)
      PARAMETER(MPI_ERR_DUP_DATAREP = 28)
      PARAMETER(MPI_ERR_FILE_EXISTS = 29)
      PARAMETER(MPI_ERR_FILE_IN_USE = 30)
      PARAMETER(MPI_ERR_FILE = 31)
      PARAMETER(MPI_ERR_INFO_KEY = 32)
      PARAMETER(MPI_ERR_INFO_NOKEY = 33)
      PARAMETER(MPI_ERR_INFO_VALUE = 34)
      PARAMETER(MPI_ERR_INFO = 35)
      PARAMETER(MPI_ERR_IO = 36)
      PARAMETER(MPI_ERR_KEYVAL = 37)
      PARAMETER(MPI_ERR_LOCKTYPE = 38)
      PARAMETER(MPI_ERR_NAME = 39)
      PARAMETER(MPI_ERR_NO_MEM = 40)
      PARAMETER(MPI_ERR_NOT_SAME = 41)
      PARAMETER(MPI_ERR_NO_SPACE = 42)
      PARAMETER(MPI_ERR_NO_SUCH_FILE = 43)
      PARAMETER(MPI_ERR_PORT = 44)
      PARAMETER(MPI_ERR_QUOTA = 45)
      PARAMETER(MPI_ERR_READ_ONLY = 46)
      PARAMETER(MPI_ERR_RMA_CONFLICT = 47)
      PARAMETER(MPI_ERR_RMA_SYNC = 48)
      PARAMETER(MPI_ERR_SERVICE = 49)
      PARAMETER(MPI_ERR_SIZE = 50)
      PARAMETER(MPI_ERR_SPAWN = 51)
      PARAMETER(MPI_ERR_UNSUPPORTED_DATAREP = 52)
      PARAMETER(MPI_ERR_UNSUPPORTED_OPERATION = 53)
      PARAMETER(MPI_ERR_WIN = 54)
      PARAMETER(MPI_ERR_LASTCODE = 55)

      EXTERNAL MPI_DUP_FN
      EXTERNAL MPI_NULL_COPY_FN
      EXTERNAL MPI_NULL_DELETE_FN