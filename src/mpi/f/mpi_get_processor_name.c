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



#include "internal/mpif.h"

#if defined(HAVE_PRAGMA_WEAK)

#pragma weak PMPI_GET_PROCESSOR_NAME = mpi_get_processor_name_f
#pragma weak pmpi_get_processor_name = mpi_get_processor_name_f
#pragma weak pmpi_get_processor_name_ = mpi_get_processor_name_f
#pragma weak pmpi_get_processor_name__ = mpi_get_processor_name_f

#pragma weak MPI_GET_PROCESSOR_NAME = mpi_get_processor_name_f
#pragma weak mpi_get_processor_name = mpi_get_processor_name_f
#pragma weak mpi_get_processor_name_ = mpi_get_processor_name_f
#pragma weak mpi_get_processor_name__ = mpi_get_processor_name_f

void mpi_get_processor_name_f(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

#else

void PMPI_GET_PROCESSOR_NAME(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void pmpi_get_processor_name(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void pmpi_get_processor_name_(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void pmpi_get_processor_name__(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void MPI_GET_PROCESSOR_NAME(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void mpi_get_processor_name(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void mpi_get_processor_name_(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

void mpi_get_processor_name__(char *name, MPI_Fint *resultlen,
                              MPI_Fint *rc)
{
    *rc = MPI_Get_processor_name(name, resultlen);
}

#endif
