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

#pragma weak PMPI_REDUCE = mpi_reduce_f
#pragma weak pmpi_reduce = mpi_reduce_f
#pragma weak pmpi_reduce_ = mpi_reduce_f
#pragma weak pmpi_reduce__ = mpi_reduce_f

#pragma weak MPI_REDUCE = mpi_reduce_f
#pragma weak mpi_reduce = mpi_reduce_f
#pragma weak mpi_reduce_ = mpi_reduce_f
#pragma weak mpi_reduce__ = mpi_reduce_f

void mpi_reduce_f(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

#else

void PMPI_REDUCE(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void pmpi_reduce(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void pmpi_reduce_(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void pmpi_reduce__(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void MPI_REDUCE(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void mpi_reduce(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void mpi_reduce_(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

void mpi_reduce__(void *sendbuf, void *recvbuf, MPI_Fint *count,
                  MPI_Fint *type, MPI_Fint *op, MPI_Fint *root,
                  MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Op c_op = MPI_Op_f2c(*op);

    *rc = MPI_Reduce(sendbuf, recvbuf, *count, c_type, c_op, *root, *comm);
}

#endif
