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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal/malloc.h"
#include "internal/mpif.h"

void mpi_waitall_f(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    MPI_Request *c_req;
    MPI_Status *c_status_array;
    int i;

    c_req = ulm_malloc(*count * sizeof(MPI_Request));
    if (c_req == NULL) {
        *rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, *rc, __FILE__, __LINE__);
        return;
    }

    for (i = 0; i < *count; i++) {
        c_req[i] = MPI_Request_f2c(request_array[i]);
    }

    if (MPI_Statuses_f2c(status_array) == MPI_STATUSES_IGNORE) {
        c_status_array = ulm_malloc(*count * sizeof(MPI_Status));
        if (c_status_array == NULL) {
            *rc = MPI_ERR_INTERN;
            _mpi_errhandler(MPI_COMM_WORLD, *rc, __FILE__, __LINE__);
            return;
        }
    } else {
        c_status_array = status_array;
    }

    *rc = MPI_Waitall(*count, c_req, c_status_array);

    if (*rc == MPI_SUCCESS) {
        /*
         * Remove request table entries for non-persistent requests
         */
        for (i = 0; i < *count; i++) {
            if ((request_array[i] >= 0)
                && (c_status_array[i]._persistent == 0)) {
                _mpi_ptr_table_free(_mpif.request_table, request_array[i]);
                request_array[i] = -1; /* MPI_REQUEST_NULL */
            }
        }
    }

    if (c_req) {
        ulm_free(c_req);
    }
    if (c_status_array != status_array) {
        ulm_free(c_status_array);
    }
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_WAITALL = mpi_waitall_f
#pragma weak pmpi_waitall = mpi_waitall_f
#pragma weak pmpi_waitall_ = mpi_waitall_f
#pragma weak pmpi_waitall__ = mpi_waitall_f

#pragma weak MPI_WAITALL = mpi_waitall_f
#pragma weak mpi_waitall = mpi_waitall_f
#pragma weak mpi_waitall_ = mpi_waitall_f
#pragma weak mpi_waitall__ = mpi_waitall_f

#else

void PMPI_WAITALL(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void pmpi_waitall(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void pmpi_waitall_(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void pmpi_waitall__(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void MPI_WAITALL(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void mpi_waitall(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void mpi_waitall_(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

void mpi_waitall__(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Status *status_array, MPI_Fint *rc)
{
    mpi_waitall_f(count, request_array, status_array, rc);
}

#endif
