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

#pragma weak PMPI_SSEND = mpi_ssend_f
#pragma weak pmpi_ssend = mpi_ssend_f
#pragma weak pmpi_ssend_ = mpi_ssend_f
#pragma weak pmpi_ssend__ = mpi_ssend_f

#pragma weak MPI_SSEND = mpi_ssend_f
#pragma weak mpi_ssend = mpi_ssend_f
#pragma weak mpi_ssend_ = mpi_ssend_f
#pragma weak mpi_ssend__ = mpi_ssend_f

void mpi_ssend_f(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

#else

void PMPI_SSEND(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void pmpi_ssend(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void pmpi_ssend_(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void pmpi_ssend__(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void MPI_SSEND(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void mpi_ssend(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void mpi_ssend_(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

void mpi_ssend__(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Ssend(buf, *count, c_type, *dest, *tag, *comm);
}

#endif
