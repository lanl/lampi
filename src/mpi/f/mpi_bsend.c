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

void mpi_bsend_f(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);

    *rc = MPI_Bsend(buf, *count, c_type, *dest, *tag, *comm);
}

#if defined(HAVE_PRAGMA_WEAK)

#pragma weak PMPI_BSEND = mpi_bsend_f
#pragma weak pmpi_bsend = mpi_bsend_f
#pragma weak pmpi_bsend_ = mpi_bsend_f
#pragma weak pmpi_bsend__ = mpi_bsend_f

#pragma weak MPI_BSEND = mpi_bsend_f
#pragma weak mpi_bsend = mpi_bsend_f
#pragma weak mpi_bsend_ = mpi_bsend_f
#pragma weak mpi_bsend__ = mpi_bsend_f

#else

void PMPI_BSEND(void *buf, MPI_Fint *count, MPI_Fint *type,
                MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void pmpi_bsend(void *buf, MPI_Fint *count, MPI_Fint *type,
                MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void pmpi_bsend_(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void pmpi_bsend__(void *buf, MPI_Fint *count, MPI_Fint *type,
                  MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                  MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void MPI_BSEND(void *buf, MPI_Fint *count, MPI_Fint *type,
                MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void mpi_bsend(void *buf, MPI_Fint *count, MPI_Fint *type,
                MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void mpi_bsend_(void *buf, MPI_Fint *count, MPI_Fint *type,
                 MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                 MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

void mpi_bsend__(void *buf, MPI_Fint *count, MPI_Fint *type,
                  MPI_Fint *dest, MPI_Fint *tag, MPI_Comm *comm,
                  MPI_Fint *rc)
{
    mpi_bsend_f(buf, count, type, dest, tag, comm, rc);
}

#endif
