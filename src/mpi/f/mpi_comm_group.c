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

#pragma weak PMPI_COMM_GROUP = mpi_comm_group_f
#pragma weak pmpi_comm_group = mpi_comm_group_f
#pragma weak pmpi_comm_group_ = mpi_comm_group_f
#pragma weak pmpi_comm_group__ = mpi_comm_group_f

#pragma weak MPI_COMM_GROUP = mpi_comm_group_f
#pragma weak mpi_comm_group = mpi_comm_group_f
#pragma weak mpi_comm_group_ = mpi_comm_group_f
#pragma weak mpi_comm_group__ = mpi_comm_group_f

void mpi_comm_group_f(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

#else

void PMPI_COMM_GROUP(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void pmpi_comm_group(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void pmpi_comm_group_(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void pmpi_comm_group__(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void MPI_COMM_GROUP(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void mpi_comm_group(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void mpi_comm_group_(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

void mpi_comm_group__(MPI_Comm *comm, MPI_Group * group, MPI_Fint *rc)
{
    *rc = MPI_Comm_group(*comm, group);
}

#endif
