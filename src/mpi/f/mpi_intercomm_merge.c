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

#pragma weak PMPI_INTERCOMM_MERGE = mpi_intercomm_merge_f
#pragma weak pmpi_intercomm_merge = mpi_intercomm_merge_f
#pragma weak pmpi_intercomm_merge_ = mpi_intercomm_merge_f
#pragma weak pmpi_intercomm_merge__ = mpi_intercomm_merge_f

#pragma weak MPI_INTERCOMM_MERGE = mpi_intercomm_merge_f
#pragma weak mpi_intercomm_merge = mpi_intercomm_merge_f
#pragma weak mpi_intercomm_merge_ = mpi_intercomm_merge_f
#pragma weak mpi_intercomm_merge__ = mpi_intercomm_merge_f

void mpi_intercomm_merge_f(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

#else

void PMPI_INTERCOMM_MERGE(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void pmpi_intercomm_merge(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void pmpi_intercomm_merge_(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void pmpi_intercomm_merge__(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void MPI_INTERCOMM_MERGE(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void mpi_intercomm_merge(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void mpi_intercomm_merge_(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

void mpi_intercomm_merge__(MPI_Fint *intercomm, MPI_Fint *high,
                           MPI_Fint *newintercomm, MPI_Fint *rc)
{
    *rc = MPI_Intercomm_merge(*intercomm, *high, newintercomm);
}

#endif
