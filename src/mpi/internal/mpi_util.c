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

#include "internal/mpi.h"
#include "internal/cLock.h"
#include "os/atomic.h"

/*
 * Function to translate ULM error codes into MPI error codes
 */
int _mpi_error(int ulm_error)
{
    static struct {
	int mpi_error;
	int ulm_error;
    } table[] = {
	{ MPI_SUCCESS, ULM_SUCCESS },
	{ MPI_ERR_NO_MEM, ULM_ERR_OUT_OF_RESOURCE },
	{ MPI_ERR_OTHER, ULM_ERR_TEMP_OUT_OF_RESOURCE },
	{ MPI_ERR_OTHER, ULM_ERR_RESOURCE_BUSY },
	{ MPI_ERR_ARG, ULM_ERR_BAD_PARAM },
	{ MPI_SUCCESS, ULM_ERR_RECV_LESS_THAN_POSTED },
	{ MPI_ERR_TRUNCATE, ULM_ERR_RECV_MORE_THAN_POSTED },
	{ MPI_ERR_PENDING, ULM_ERR_NO_MATCH_YET },
	{ MPI_ERR_INTERN, ULM_ERR_FATAL },
	{ MPI_ERR_BUFFER, ULM_ERR_BUFFER },
	{ MPI_ERR_COMM, ULM_ERR_COMM },
	{ MPI_ERR_RANK, ULM_ERR_RANK },
	{ MPI_ERR_REQUEST, ULM_ERR_REQUEST },
	{ MPI_ERR_UNKNOWN, ULM_ERROR }	/* table terminator */
    };
    int i;

    /* check to see if this is already an MPI error code */
    if (ulm_error > 0) {
        return ulm_error;
    }

    for (i = 0; table[i].ulm_error != ULM_ERROR; i++) {
	if (ulm_error == table[i].ulm_error) {
	    return table[i].mpi_error;
	}
    }
    return MPI_ERR_UNKNOWN;
}


/*
 * print for debugging
 */
void _mpi_dbg(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fflush(stderr);
    va_end(ap);
}


/*
 * locks
 */
void _mpi_lock(lockStructure_t *lockData)
{
    if (_mpi.threadsafe) {
        lock(lockData);
    }
}

void _mpi_unlock(lockStructure_t *lockData)
{
    if (_mpi.threadsafe) {
        unlock(lockData);
    }
}
