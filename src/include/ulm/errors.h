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

#ifndef _ULM_ERRORS_H_
#define _ULM_ERRORS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * WARNING WARNING WARNING - all ULM error codes must be less than
 * zero, and all MPI error codes must be greater than zero for
 * _mpi_error() to work correctly! ULM_SUCCESS = MPI_SUCCESS = 0!
 */

/* error codes */
enum {
    ULM_SUCCESS = 0,
    ULM_ERROR = -1,
    ULM_ERR_OUT_OF_RESOURCE = -2,
    ULM_ERR_TEMP_OUT_OF_RESOURCE = -3,
    ULM_ERR_RESOURCE_BUSY = -4,
    ULM_ERR_BAD_PARAM = -5,     /* equivalent to MPI_ERR_ARG error code */
    ULM_ERR_RECV_LESS_THAN_POSTED = -6,
    ULM_ERR_RECV_MORE_THAN_POSTED = -7,
    ULM_ERR_NO_MATCH_YET = -8,
    ULM_ERR_FATAL = -9,
    ULM_ERR_BUFFER = -10,       /* maps to MPI_ERR_BUFFER error code */
    ULM_ERR_COMM = -11,         /* maps to MPI_ERR_COMM error code */
    ULM_ERR_RANK = -12,         /* maps to MPI_ERR_RANK error code */
    ULM_ERR_REQUEST = -13,      /* maps to MPI_ERR_REQUEST error code */
    ULM_ERR_UNKNOWN = -14,
    ULM_ERR_BAD_PATH = -15,	/* message must be rebound to new path */
    ULM_ERR_BAD_SUBPATH = -16,	/* message needs new device for this path */
#ifdef USE_ELAN_COLL
    ULM_ERR_BCAST_SEND = -17,	/* Error Sending   */
    ULM_ERR_BCAST_RECV = -18,	/* Error Receiving */
    ULM_ERR_BCAST_SYNC = -19,   /* Error Sync      */
    ULM_ERR_BCAST_FAIL  = -20,	/* Bcast Failed, hw/bcast unavailable */
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* !_ULM_ERRORS_H_ */
