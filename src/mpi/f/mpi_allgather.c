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

void mpi_allgather_f(void *sendbuf,
                     MPI_Fint *sendcount, MPI_Fint *sendtype,
                     void *recvbuf, MPI_Fint *recvcount,
                     MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    MPI_Datatype c_sendtype = MPI_Type_f2c(*sendtype);
    MPI_Datatype c_recvtype = MPI_Type_f2c(*recvtype);

    *rc = MPI_Allgather(sendbuf,
                        *sendcount, c_sendtype, recvbuf,
                        *recvcount, c_recvtype, *comm);
}


#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_ALLGATHER = mpi_allgather_f
#pragma weak pmpi_allgather = mpi_allgather_f
#pragma weak pmpi_allgather_ = mpi_allgather_f
#pragma weak pmpi_allgather__ = mpi_allgather_f

#pragma weak MPI_ALLGATHER = mpi_allgather_f
#pragma weak mpi_allgather = mpi_allgather_f
#pragma weak mpi_allgather_ = mpi_allgather_f
#pragma weak mpi_allgather__ = mpi_allgather_f

#else

void PMPI_ALLGATHER(void *sendbuf,
                    MPI_Fint *sendcount, MPI_Fint *sendtype,
                    void *recvbuf, MPI_Fint *recvcount,
                    MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void pmpi_allgather(void *sendbuf,
                    MPI_Fint *sendcount, MPI_Fint *sendtype,
                    void *recvbuf, MPI_Fint *recvcount,
                    MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void pmpi_allgather_(void *sendbuf,
                     MPI_Fint *sendcount, MPI_Fint *sendtype,
                     void *recvbuf, MPI_Fint *recvcount,
                     MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void pmpi_allgather__(void *sendbuf,
                      MPI_Fint *sendcount, MPI_Fint *sendtype,
                      void *recvbuf, MPI_Fint *recvcount,
                      MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}


void MPI_ALLGATHER(void *sendbuf,
                    MPI_Fint *sendcount, MPI_Fint *sendtype,
                    void *recvbuf, MPI_Fint *recvcount,
                    MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void mpi_allgather(void *sendbuf,
                    MPI_Fint *sendcount, MPI_Fint *sendtype,
                    void *recvbuf, MPI_Fint *recvcount,
                    MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void mpi_allgather_(void *sendbuf,
                     MPI_Fint *sendcount, MPI_Fint *sendtype,
                     void *recvbuf, MPI_Fint *recvcount,
                     MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}

void mpi_allgather__(void *sendbuf,
                      MPI_Fint *sendcount, MPI_Fint *sendtype,
                      void *recvbuf, MPI_Fint *recvcount,
                      MPI_Fint *recvtype, MPI_Comm *comm, MPI_Fint *rc)
{
    mpi_allgather_f(sendbuf, sendcount, sendtype,
                    recvbuf, recvcount, recvtype, comm, rc);
}


#endif
