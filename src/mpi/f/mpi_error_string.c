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

#include "internal/mpif.h"

/*
 * On most platforms Fortran strings are passed to C as
 * - a non-null terminated character buffer
 * - the length of the the buffer appended as an int argument at the end
 *   of the argument list
 *
 * For eccentric platforms (CRAY, IBM?) this implementation will not work.
 */

void mpi_error_string_f(MPI_Fint *errorcode, char *errstring,
                        MPI_Fint *resultlen, MPI_Fint *rc,
                        int string_length)
{
    char s[MPI_MAX_ERROR_STRING + 1];

    *rc = MPI_Error_string(*errorcode, s, resultlen);
    if (*rc == MPI_SUCCESS) {
        memset(errstring, ' ', string_length);
        memmove(errstring, s, *resultlen);
    }
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_ERROR_STRING = mpi_error_string_f
#pragma weak pmpi_error_string = mpi_error_string_f
#pragma weak pmpi_error_string_ = mpi_error_string_f
#pragma weak pmpi_error_string__ = mpi_error_string_f

#pragma weak MPI_ERROR_STRING = mpi_error_string_f
#pragma weak mpi_error_string = mpi_error_string_f
#pragma weak mpi_error_string_ = mpi_error_string_f
#pragma weak mpi_error_string__ = mpi_error_string_f

#else

void PMPI_ERROR_STRING(MPI_Fint *errorcode, char *errstring,
                       MPI_Fint *resultlen, MPI_Fint *rc,
                       int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void pmpi_error_string(MPI_Fint *errorcode, char *errstring,
                       MPI_Fint *resultlen, MPI_Fint *rc,
                       int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void pmpi_error_string_(MPI_Fint *errorcode, char *errstring,
                        MPI_Fint *resultlen, MPI_Fint *rc,
                        int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void pmpi_error_string__(MPI_Fint *errorcode, char *errstring,
                         MPI_Fint *resultlen, MPI_Fint *rc,
                         int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void MPI_ERROR_STRING(MPI_Fint *errorcode, char *errstring,
                       MPI_Fint *resultlen, MPI_Fint *rc,
                       int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void mpi_error_string(MPI_Fint *errorcode, char *errstring,
                       MPI_Fint *resultlen, MPI_Fint *rc,
                       int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void mpi_error_string_(MPI_Fint *errorcode, char *errstring,
                        MPI_Fint *resultlen, MPI_Fint *rc,
                        int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

void mpi_error_string__(MPI_Fint *errorcode, char *errstring,
                         MPI_Fint *resultlen, MPI_Fint *rc,
                         int string_length)
{
    mpi_error_string_f(errorcode, errstring, resultlen, rc, string_length);
}

#endif
