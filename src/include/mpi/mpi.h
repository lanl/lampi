/*
 * Copyright 2002-2004. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef MPI_MPI_H_INCLUDED
#define MPI_MPI_H_INCLUDED

#include "mpi/types.h"
#include "mpi/constants.h"

/*
 * Basic data types
 */

extern ULMType_t ulm_type[ULM_NUMBER_OF_TYPES];

#define ULM_TYPE(INDEX) ((MPI_Datatype) &(ulm_type[INDEX]))

#define MPI_CHAR                ULM_TYPE(ULM_CHAR)
#define MPI_SHORT               ULM_TYPE(ULM_SHORT)
#define MPI_INT                 ULM_TYPE(ULM_INT)
#define MPI_LONG                ULM_TYPE(ULM_LONG)
#define MPI_UNSIGNED_CHAR       ULM_TYPE(ULM_UNSIGNED_CHAR)
#define MPI_UNSIGNED_SHORT      ULM_TYPE(ULM_UNSIGNED_SHORT)
#define MPI_UNSIGNED            ULM_TYPE(ULM_UNSIGNED)
#define MPI_UNSIGNED_LONG       ULM_TYPE(ULM_UNSIGNED_LONG)
#define MPI_FLOAT               ULM_TYPE(ULM_FLOAT)
#define MPI_DOUBLE              ULM_TYPE(ULM_DOUBLE)
#define MPI_LONG_DOUBLE         ULM_TYPE(ULM_LONG_DOUBLE)
#define MPI_PACKED              ULM_TYPE(ULM_PACKED)
#define MPI_BYTE                ULM_TYPE(ULM_BYTE)
#define MPI_LONG_LONG_INT       ULM_TYPE(ULM_LONG_LONG_INT)
#define MPI_FLOAT_INT           ULM_TYPE(ULM_FLOAT_INT)
#define MPI_DOUBLE_INT          ULM_TYPE(ULM_DOUBLE_INT)
#define MPI_LONG_INT            ULM_TYPE(ULM_LONG_INT)
#define MPI_2INT                ULM_TYPE(ULM_2INT)
#define MPI_SHORT_INT           ULM_TYPE(ULM_SHORT_INT)
#define MPI_LONG_DOUBLE_INT     ULM_TYPE(ULM_LONG_DOUBLE_INT)
#define MPI_LB                  ULM_TYPE(ULM_LB)
#define MPI_UB                  ULM_TYPE(ULM_UB)
#define MPI_INTEGER             ULM_TYPE(ULM_INT)
#define MPI_REAL                ULM_TYPE(ULM_FLOAT)
#define MPI_DOUBLE_PRECISION    ULM_TYPE(ULM_DOUBLE)
#define MPI_LOGICAL             ULM_TYPE(ULM_UNSIGNED)
#define MPI_CHARACTER           ULM_TYPE(ULM_CHAR)
#define MPI_INTEGER1            ULM_TYPE(ULM_INTEGER1)
#define MPI_INTEGER2            ULM_TYPE(ULM_INTEGER2)
#define MPI_INTEGER4            ULM_TYPE(ULM_INTEGER4)
#define MPI_INTEGER8            ULM_TYPE(ULM_INTEGER8)
#define MPI_REAL2               ULM_TYPE(ULM_REAL2)
#define MPI_REAL4               ULM_TYPE(ULM_REAL4)
#define MPI_REAL8               ULM_TYPE(ULM_REAL8)
#define MPI_COMPLEX             ULM_TYPE(ULM_COMPLEX)
#define MPI_DOUBLE_COMPLEX      ULM_TYPE(ULM_DOUBLE_COMPLEX)
#define MPI_2REAL               ULM_TYPE(ULM_2REAL)
#define MPI_2DOUBLE_PRECISION   ULM_TYPE(ULM_2DOUBLE_PRECISION)
#define MPI_2INTEGER            ULM_TYPE(ULM_2INTEGER)

/*
 * Basic reduction operations
 */

extern ULMOp_t ULM_MAX, ULM_MIN, ULM_SUM, ULM_PROD, ULM_MAXLOC, ULM_MINLOC,
               ULM_BAND, ULM_BOR, ULM_BXOR, ULM_LAND, ULM_LOR, ULM_LXOR;

#define MPI_MAX     ((MPI_Op) &ULM_MAX)
#define MPI_MIN     ((MPI_Op) &ULM_MIN)
#define MPI_SUM     ((MPI_Op) &ULM_SUM)
#define MPI_PROD    ((MPI_Op) &ULM_PROD)
#define MPI_MAXLOC  ((MPI_Op) &ULM_MAXLOC)
#define MPI_MINLOC  ((MPI_Op) &ULM_MINLOC)
#define MPI_BAND    ((MPI_Op) &ULM_BAND)
#define MPI_BOR     ((MPI_Op) &ULM_BOR)
#define MPI_BXOR    ((MPI_Op) &ULM_BXOR)
#define MPI_LAND    ((MPI_Op) &ULM_LAND)
#define MPI_LOR     ((MPI_Op) &ULM_LOR)
#define MPI_LXOR    ((MPI_Op) &ULM_LXOR)


#ifdef __cplusplus
extern "C"
{
#endif

/*
 * MPI API
 */
int MPI_Abort(MPI_Comm, int);
int MPI_Address(void *, MPI_Aint *);
int MPI_Allgather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int MPI_Allgatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int MPI_Allreduce(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int MPI_Alltoall(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int MPI_Alltoallv(void *, int *, int *, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int MPI_Alltoallw(void *, int *, int *, MPI_Datatype *, void *, int *, int *, MPI_Datatype *, MPI_Comm);
int MPI_Attr_delete(MPI_Comm, int);
int MPI_Attr_get(MPI_Comm, int, void *, int *);
int MPI_Attr_put(MPI_Comm, int, void *);
int MPI_Barrier(MPI_Comm);
int MPI_Bcast(void *, int, MPI_Datatype, int, MPI_Comm);
int MPI_Bsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int MPI_Bsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Buffer_attach(void *, int);
int MPI_Buffer_detach(void *, int *);
int MPI_Cancel(MPI_Request *);
int MPI_Cart_coords(MPI_Comm, int, int, int *);
int MPI_Cart_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int MPI_Cart_get(MPI_Comm, int, int *, int *, int *);
int MPI_Cart_map(MPI_Comm, int, int *, int *, int *);
int MPI_Cart_rank(MPI_Comm, int *, int *);
int MPI_Cart_shift(MPI_Comm, int, int, int *, int *);
int MPI_Cart_sub(MPI_Comm, int *, MPI_Comm *);
int MPI_Cartdim_get(MPI_Comm, int *);
int MPI_Comm_compare(MPI_Comm, MPI_Comm, int *);
int MPI_Comm_create(MPI_Comm, MPI_Group, MPI_Comm *);
int MPI_Comm_create_errhandler(MPI_Comm_errhandler_fn *, MPI_Errhandler *);
int MPI_Comm_dup(MPI_Comm, MPI_Comm *);
int MPI_Comm_free(MPI_Comm *);
int MPI_Comm_free_errhandler(MPI_Errhandler *);
int MPI_Comm_get_errhandler(MPI_Comm, MPI_Errhandler *);
int MPI_Comm_get_name(MPI_Comm, char *, int *);
int MPI_Comm_group(MPI_Comm, MPI_Group *);
int MPI_Comm_rank(MPI_Comm, int *);
int MPI_Comm_remote_group(MPI_Comm, MPI_Group *);
int MPI_Comm_remote_size(MPI_Comm, int *);
int MPI_Comm_set_errhandler(MPI_Comm, MPI_Errhandler);
int MPI_Comm_set_name(MPI_Comm, char *);
int MPI_Comm_size(MPI_Comm, int *);
int MPI_Comm_split(MPI_Comm, int, int, MPI_Comm *);
int MPI_Comm_test_inter(MPI_Comm, int *);
int MPI_Dims_create(int, int, int *);
int MPI_Errhandler_create(MPI_Handler_function *, MPI_Errhandler *);
int MPI_Errhandler_free(MPI_Errhandler *);
int MPI_Errhandler_get(MPI_Comm, MPI_Errhandler *);
int MPI_Errhandler_set(MPI_Comm, MPI_Errhandler);
int MPI_Error_class(int, int *);
int MPI_Error_string(int, char *, int *);
int MPI_Finalize(void);
int MPI_Finalized(int *);
int MPI_Gather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int MPI_Gatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, int, MPI_Comm);
int MPI_Get_count(MPI_Status *, MPI_Datatype, int *);
int MPI_Get_elements(MPI_Status *, MPI_Datatype, int *);
int MPI_Get_processor_name(char *, int *);
int MPI_Get_version(int *, int *);
int MPI_Graph_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int MPI_Graph_get(MPI_Comm, int, int, int *, int *);
int MPI_Graph_map(MPI_Comm, int, int *, int *, int *);
int MPI_Graph_neighbors(MPI_Comm, int, int, int *);
int MPI_Graph_neighbors_count(MPI_Comm, int, int *);
int MPI_Graphdims_get(MPI_Comm, int *, int *);
int MPI_Group_compare(MPI_Group, MPI_Group, int *);
int MPI_Group_difference(MPI_Group, MPI_Group, MPI_Group *);
int MPI_Group_excl(MPI_Group, int, int *, MPI_Group *);
int MPI_Group_free(MPI_Group *);
int MPI_Group_incl(MPI_Group, int, int *, MPI_Group *);
int MPI_Group_intersection(MPI_Group, MPI_Group, MPI_Group *);
int MPI_Group_range_excl(MPI_Group, int, int[][3], MPI_Group *);
int MPI_Group_range_incl(MPI_Group, int, int[][3], MPI_Group *);
int MPI_Group_rank(MPI_Group, int *);
int MPI_Group_size(MPI_Group, int *);
int MPI_Group_translate_ranks(MPI_Group, int, int *, MPI_Group, int *);
int MPI_Group_union(MPI_Group, MPI_Group, MPI_Group *);
int MPI_Ibsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Init(int *, char ***);
int MPI_Init_thread(int *, char ***, int, int *);
int MPI_Initialized(int *);
int MPI_Intercomm_create(MPI_Comm, int, MPI_Comm, int, int, MPI_Comm *);
int MPI_Intercomm_merge(MPI_Comm, int, MPI_Comm *);
int MPI_Iprobe(int, int, MPI_Comm, int *, MPI_Status *);
int MPI_Irecv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Irsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Isend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Issend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Keyval_create(MPI_Copy_function *, MPI_Delete_function *, int *, void *);
int MPI_Keyval_free(int *);
int MPI_Op_create(MPI_User_function *, int, MPI_Op *);
int MPI_Op_free(MPI_Op *);
int MPI_Pack(void *, int, MPI_Datatype, void *, int, int *, MPI_Comm);
int MPI_Pack_size(int, MPI_Datatype, MPI_Comm, int *);
int MPI_Pcontrol(const int, ...);
int MPI_Probe(int, int, MPI_Comm, MPI_Status *);
int MPI_Query_thread(int *);
int MPI_Recv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int MPI_Recv_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Reduce(void *, void *, int, MPI_Datatype, MPI_Op, int, MPI_Comm);
int MPI_Reduce_scatter(void *, void *, int *, MPI_Datatype, MPI_Op, MPI_Comm);
int MPI_Request_free(MPI_Request *);
int MPI_Rsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int MPI_Rsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Scan(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int MPI_Scatter(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int MPI_Scatterv(void *, int *, int *, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int MPI_Send(void *, int, MPI_Datatype, int, int, MPI_Comm);
int MPI_Send_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Sendrecv(void *, int, MPI_Datatype, int, int, void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int MPI_Sendrecv_replace(void *, int, MPI_Datatype, int, int, int, int, MPI_Comm, MPI_Status *);
int MPI_Ssend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int MPI_Ssend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int MPI_Start(MPI_Request *);
int MPI_Startall(int, MPI_Request *);
int MPI_Status_set_cancelled(MPI_Status *, int);
int MPI_Status_set_elements(MPI_Status *, MPI_Datatype, int);
int MPI_Test(MPI_Request *, int *, MPI_Status *);
int MPI_Test_cancelled(MPI_Status *, int *);
int MPI_Testall(int, MPI_Request *, int *, MPI_Status *);
int MPI_Testany(int, MPI_Request *, int *, int *, MPI_Status *);
int MPI_Testsome(int, MPI_Request *, int *, int *, MPI_Status *);
int MPI_Topo_test(MPI_Comm, int *);
int MPI_Type_commit(MPI_Datatype *);
int MPI_Type_contiguous(int, MPI_Datatype, MPI_Datatype *);
int MPI_Type_count(MPI_Datatype, int *);
int MPI_Type_create_darray(int, int, int, int *, int *, int *, int *, int, MPI_Datatype, MPI_Datatype *);
int MPI_Type_create_subarray(int, int *, int *, int *, int, MPI_Datatype, MPI_Datatype *);
int MPI_Type_extent(MPI_Datatype, MPI_Aint *);
int MPI_Type_free(MPI_Datatype *);
int MPI_Type_get_contents(MPI_Datatype, int, int, int, int *, MPI_Aint *, MPI_Datatype *);
int MPI_Type_get_envelope(MPI_Datatype, int *, int *, int *, int *);
int MPI_Type_get_name(MPI_Datatype, char *, int *);
int MPI_Type_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);
int MPI_Type_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);
int MPI_Type_indexed(int, int *, int *, MPI_Datatype, MPI_Datatype *);
int MPI_Type_lb(MPI_Datatype, MPI_Aint *);
int MPI_Type_set_name(MPI_Datatype, char *);
int MPI_Type_size(MPI_Datatype, int *);
int MPI_Type_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);
int MPI_Type_ub(MPI_Datatype, MPI_Aint *);
int MPI_Type_vector(int, int, int, MPI_Datatype, MPI_Datatype *);
int MPI_Unpack(void *, int, int *, void *, int, MPI_Datatype, MPI_Comm);
int MPI_Wait(MPI_Request *, MPI_Status *);
int MPI_Waitall(int, MPI_Request *, MPI_Status *);
int MPI_Waitany(int, MPI_Request *, int *, MPI_Status *);
int MPI_Waitsome(int, MPI_Request *, int *, int *, MPI_Status *);
int MPI_Win_create_errhandler(MPI_Win_errhandler_fn *, MPI_Errhandler *);
int MPI_Win_free_errhandler(MPI_Errhandler *);
int MPI_Win_get_errhandler(MPI_Win, MPI_Errhandler *);
int MPI_Win_set_errhandler(MPI_Win, MPI_Errhandler);
double MPI_Wtick(void);
double MPI_Wtime(void);

/*
 * MPI Profiling API
 */
int PMPI_Abort(MPI_Comm, int);
int PMPI_Address(void *, MPI_Aint *);
int PMPI_Allgather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Allgatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int PMPI_Allreduce(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Alltoall(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Alltoallv(void *, int *, int *, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int PMPI_Alltoallw(void *, int *, int *, MPI_Datatype *, void *, int *, int *, MPI_Datatype *, MPI_Comm);
int PMPI_Attr_delete(MPI_Comm, int);
int PMPI_Attr_get(MPI_Comm, int, void *, int *);
int PMPI_Attr_put(MPI_Comm, int, void *);
int PMPI_Barrier(MPI_Comm);
int PMPI_Bcast(void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Bsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Bsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Buffer_attach(void *, int);
int PMPI_Buffer_detach(void *, int *);
int PMPI_Cancel(MPI_Request *);
int PMPI_Cart_coords(MPI_Comm, int, int, int *);
int PMPI_Cart_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int PMPI_Cart_get(MPI_Comm, int, int *, int *, int *);
int PMPI_Cart_map(MPI_Comm, int, int *, int *, int *);
int PMPI_Cart_rank(MPI_Comm, int *, int *);
int PMPI_Cart_shift(MPI_Comm, int, int, int *, int *);
int PMPI_Cart_sub(MPI_Comm, int *, MPI_Comm *);
int PMPI_Cartdim_get(MPI_Comm, int *);
int PMPI_Comm_compare(MPI_Comm, MPI_Comm, int *);
int PMPI_Comm_create(MPI_Comm, MPI_Group, MPI_Comm *);
int PMPI_Comm_create_errhandler(MPI_Comm_errhandler_fn *, MPI_Errhandler *);
int PMPI_Comm_dup(MPI_Comm, MPI_Comm *);
int PMPI_Comm_free(MPI_Comm *);
int PMPI_Comm_free_errhandler(MPI_Errhandler *);
int PMPI_Comm_get_errhandler(MPI_Comm, MPI_Errhandler *);
int PMPI_Comm_get_name(MPI_Comm, char *, int *);
int PMPI_Comm_group(MPI_Comm, MPI_Group *);
int PMPI_Comm_rank(MPI_Comm, int *);
int PMPI_Comm_remote_group(MPI_Comm, MPI_Group *);
int PMPI_Comm_remote_size(MPI_Comm, int *);
int PMPI_Comm_set_errhandler(MPI_Comm, MPI_Errhandler);
int PMPI_Comm_set_name(MPI_Comm, char *);
int PMPI_Comm_size(MPI_Comm, int *);
int PMPI_Comm_split(MPI_Comm, int, int, MPI_Comm *);
int PMPI_Comm_test_inter(MPI_Comm, int *);
int PMPI_Dims_create(int, int, int *);
int PMPI_Errhandler_create(MPI_Handler_function *, MPI_Errhandler *);
int PMPI_Errhandler_free(MPI_Errhandler *);
int PMPI_Errhandler_get(MPI_Comm, MPI_Errhandler *);
int PMPI_Errhandler_set(MPI_Comm, MPI_Errhandler);
int PMPI_Error_class(int, int *);
int PMPI_Error_string(int, char *, int *);
int PMPI_Finalize(void);
int PMPI_Finalized(int *);
int PMPI_Gather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Gatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, int, MPI_Comm);
int PMPI_Get_count(MPI_Status *, MPI_Datatype, int *);
int PMPI_Get_elements(MPI_Status *, MPI_Datatype, int *);
int PMPI_Get_processor_name(char *, int *);
int PMPI_Get_version(int *, int *);
int PMPI_Graph_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int PMPI_Graph_get(MPI_Comm, int, int, int *, int *);
int PMPI_Graph_map(MPI_Comm, int, int *, int *, int *);
int PMPI_Graph_neighbors(MPI_Comm, int, int, int *);
int PMPI_Graph_neighbors_count(MPI_Comm, int, int *);
int PMPI_Graphdims_get(MPI_Comm, int *, int *);
int PMPI_Group_compare(MPI_Group, MPI_Group, int *);
int PMPI_Group_difference(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Group_excl(MPI_Group, int, int *, MPI_Group *);
int PMPI_Group_free(MPI_Group *);
int PMPI_Group_incl(MPI_Group, int, int *, MPI_Group *);
int PMPI_Group_intersection(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Group_range_excl(MPI_Group, int, int[][3], MPI_Group *);
int PMPI_Group_range_incl(MPI_Group, int, int[][3], MPI_Group *);
int PMPI_Group_rank(MPI_Group, int *);
int PMPI_Group_size(MPI_Group, int *);
int PMPI_Group_translate_ranks(MPI_Group, int, int *, MPI_Group, int *);
int PMPI_Group_union(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Ibsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Init(int *, char ***);
int PMPI_Init_thread(int *, char ***, int, int *);
int PMPI_Initialized(int *);
int PMPI_Intercomm_create(MPI_Comm, int, MPI_Comm, int, int, MPI_Comm *);
int PMPI_Intercomm_merge(MPI_Comm, int, MPI_Comm *);
int PMPI_Iprobe(int, int, MPI_Comm, int *, MPI_Status *);
int PMPI_Irecv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Irsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Isend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Issend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Keyval_create(MPI_Copy_function *, MPI_Delete_function *, int *, void *);
int PMPI_Keyval_free(int *);
int PMPI_Op_create(MPI_User_function *, int, MPI_Op *);
int PMPI_Op_free(MPI_Op *);
int PMPI_Pack(void *, int, MPI_Datatype, void *, int, int *, MPI_Comm);
int PMPI_Pack_size(int, MPI_Datatype, MPI_Comm, int *);
int PMPI_Pcontrol(const int, ...);
int PMPI_Probe(int, int, MPI_Comm, MPI_Status *);
int PMPI_Query_thread(int *);
int PMPI_Recv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int PMPI_Recv_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Reduce(void *, void *, int, MPI_Datatype, MPI_Op, int, MPI_Comm);
int PMPI_Reduce_scatter(void *, void *, int *, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Request_free(MPI_Request *);
int PMPI_Rsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Rsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Scan(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Scatter(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Scatterv(void *, int *, int *, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Send(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Send_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Sendrecv(void *, int, MPI_Datatype, int, int, void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int PMPI_Sendrecv_replace(void *, int, MPI_Datatype, int, int, int, int, MPI_Comm, MPI_Status *);
int PMPI_Ssend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Ssend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Start(MPI_Request *);
int PMPI_Startall(int, MPI_Request *);
int PMPI_Status_set_cancelled(MPI_Status *, int);
int PMPI_Status_set_elements(MPI_Status *, MPI_Datatype, int);
int PMPI_Test(MPI_Request *, int *, MPI_Status *);
int PMPI_Test_cancelled(MPI_Status *, int *);
int PMPI_Testall(int, MPI_Request *, int *, MPI_Status *);
int PMPI_Testany(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Testsome(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Topo_test(MPI_Comm, int *);
int PMPI_Type_commit(MPI_Datatype *);
int PMPI_Type_contiguous(int, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_count(MPI_Datatype, int *);
int PMPI_Type_create_darray(int, int, int, int *, int *, int *, int *, int, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_create_subarray(int, int *, int *, int *, int, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_extent(MPI_Datatype, MPI_Aint *);
int PMPI_Type_free(MPI_Datatype *);
int PMPI_Type_get_contents(MPI_Datatype, int, int, int, int *, MPI_Aint *, MPI_Datatype *);
int PMPI_Type_get_envelope(MPI_Datatype, int *, int *, int *, int *);
int PMPI_Type_get_name(MPI_Datatype, char *, int *);
int PMPI_Type_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_indexed(int, int *, int *, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_lb(MPI_Datatype, MPI_Aint *);
int PMPI_Type_set_name(MPI_Datatype, char *);
int PMPI_Type_size(MPI_Datatype, int *);
int PMPI_Type_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);
int PMPI_Type_ub(MPI_Datatype, MPI_Aint *);
int PMPI_Type_vector(int, int, int, MPI_Datatype, MPI_Datatype *);
int PMPI_Unpack(void *, int, int *, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Wait(MPI_Request *, MPI_Status *);
int PMPI_Waitall(int, MPI_Request *, MPI_Status *);
int PMPI_Waitany(int, MPI_Request *, int *, MPI_Status *);
int PMPI_Waitsome(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Win_create_errhandler(MPI_Win_errhandler_fn *, MPI_Errhandler *);
int PMPI_Win_free_errhandler(MPI_Errhandler *);
int PMPI_Win_get_errhandler(MPI_Win, MPI_Errhandler *);
int PMPI_Win_set_errhandler(MPI_Win, MPI_Errhandler);
double PMPI_Wtick(void);
double PMPI_Wtime(void);

/*
 * C to fortran interoperability functions
 */
MPI_Fint MPI_Comm_c2f(MPI_Comm comm);
MPI_Comm MPI_Comm_f2c(MPI_Fint comm);
#define MPI_Errhandler_c2f(x)   (x)
#define MPI_Errhandler_f2c(x)   (x)
MPI_Fint MPI_Group_c2f(MPI_Group group);
MPI_Group MPI_Group_f2c(MPI_Fint group);
MPI_Fint MPI_Op_c2f(MPI_Op op);
MPI_Op MPI_Op_f2c(MPI_Fint op);
MPI_Fint MPI_Request_c2f(MPI_Request request);
MPI_Request MPI_Request_f2c(MPI_Fint request);
MPI_Fint MPI_Type_c2f(MPI_Datatype datatype);
MPI_Datatype MPI_Type_f2c(MPI_Fint datatype);
MPI_Fint MPI_Win_c2f(MPI_Win win);
MPI_Win MPI_Win_f2c(MPI_Fint win);

/*
 * default attribute function pointers
 */
MPI_Copy_function mpi_dup_fn_c;
MPI_Copy_function mpi_null_copy_fn_c;
MPI_Delete_function mpi_null_delete_fn_c;

/*
 * A macro version of MPI_Abort that reports file and line
 * information.
 */

#ifdef USE_ABORT_MACRO

int _ulm_abort(int, int, char *, int);

#define MPI_Abort(COMM, ERR)                    \
    _ulm_abort(COMM, ERR, __FILE__, __LINE__)

#define PMPI_Abort(COMM, ERR)                   \
    _ulm_abort(COMM, ERR, __FILE__, __LINE__)

#endif /* ! USE_ABORT_MACRO */

#ifdef __cplusplus
}
#endif

#endif /* MPI_MPI_H_INCLUDED */
