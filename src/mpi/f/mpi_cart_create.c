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

#pragma weak PMPI_CART_CREATE = mpi_cart_create_f
#pragma weak pmpi_cart_create = mpi_cart_create_f
#pragma weak pmpi_cart_create_ = mpi_cart_create_f
#pragma weak pmpi_cart_create__ = mpi_cart_create_f

#pragma weak MPI_CART_CREATE = mpi_cart_create_f
#pragma weak mpi_cart_create = mpi_cart_create_f
#pragma weak mpi_cart_create_ = mpi_cart_create_f
#pragma weak mpi_cart_create__ = mpi_cart_create_f

void mpi_cart_create_f(MPI_Comm *comm_old, MPI_Fint *ndims,
                       MPI_Fint *dims, MPI_Fint *periods,
                       MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

#else

void PMPI_CART_CREATE(MPI_Comm *comm_old, MPI_Fint *ndims,
                      MPI_Fint *dims, MPI_Fint *periods,
                      MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void pmpi_cart_create(MPI_Comm *comm_old, MPI_Fint *ndims,
                      MPI_Fint *dims, MPI_Fint *periods,
                      MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void pmpi_cart_create_(MPI_Comm *comm_old, MPI_Fint *ndims,
                       MPI_Fint *dims, MPI_Fint *periods,
                       MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void pmpi_cart_create__(MPI_Comm *comm_old, MPI_Fint *ndims,
                        MPI_Fint *dims, MPI_Fint *periods,
                        MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void MPI_CART_CREATE(MPI_Comm *comm_old, MPI_Fint *ndims,
                      MPI_Fint *dims, MPI_Fint *periods,
                      MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void mpi_cart_create(MPI_Comm *comm_old, MPI_Fint *ndims,
                      MPI_Fint *dims, MPI_Fint *periods,
                      MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void mpi_cart_create_(MPI_Comm *comm_old, MPI_Fint *ndims,
                       MPI_Fint *dims, MPI_Fint *periods,
                       MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

void mpi_cart_create__(MPI_Comm *comm_old, MPI_Fint *ndims,
                        MPI_Fint *dims, MPI_Fint *periods,
                        MPI_Fint *reorder, MPI_Comm *comm_new, MPI_Fint *rc)
{
    *rc = MPI_Cart_create(*comm_old, *ndims, dims, periods, *reorder,
                          comm_new);
}

#endif
