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

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_TEST_CANCELLED = mpi_test_cancelled_f
#pragma weak pmpi_test_cancelled = mpi_test_cancelled_f
#pragma weak pmpi_test_cancelled_ = mpi_test_cancelled_f
#pragma weak pmpi_test_cancelled__ = mpi_test_cancelled_f

#pragma weak MPI_TEST_CANCELLED = mpi_test_cancelled_f
#pragma weak mpi_test_cancelled = mpi_test_cancelled_f
#pragma weak mpi_test_cancelled_ = mpi_test_cancelled_f
#pragma weak mpi_test_cancelled__ = mpi_test_cancelled_f

void mpi_test_cancelled_f(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

#else

void PMPI_TEST_CANCELLED(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void pmpi_test_cancelled(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void pmpi_test_cancelled_(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void pmpi_test_cancelled__(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void MPI_TEST_CANCELLED(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void mpi_test_cancelled(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void mpi_test_cancelled_(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

void mpi_test_cancelled__(MPI_Status *status, MPI_Fint *flag, MPI_Fint *rc)
{
    *rc = MPI_Test_cancelled(status, flag);
}

#endif
