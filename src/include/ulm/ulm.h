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



#ifndef _ULM_H_
#define _ULM_H_

#include <sys/types.h>

#include "mpi/constants.h"
#include "mpi/types.h"
#include "ulm/constants.h"
#include "ulm/errors.h"
#include "ulm/system.h"
#include "ulm/types.h"

/*
 * Spin until a boolean expression is false
 */
#define ULM_SPIN_MAX 10000
#define ULM_SPIN_AND_MAKE_PROGRESS(EXPR) \
    do {                                 \
        int count = 0;                   \
        while (EXPR) {                   \
            if (count == ULM_SPIN_MAX) { \
                count = 0;               \
                ulm_make_progress();     \
            }                            \
            count++;                     \
            mb();                        \
        }                                \
    } while (0);


#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * initialization function - must be called
 *
 * \return              ULM return code
 */
int ulm_init(void);

/*!
 * initialization function - thread usage set - must be called or
 *    else ulm_init must be called
 *
 * \return              ULM return code
 */
int ulm_init_threads(int use);

/*!
 * termination function - must be called
 *
 * \return              ULM return code
 */
void ulm_finalize(void);

/* 
 * cleanup function, frees allocated memory and 
 * ensures that the proper destructor calls occur
 */
void ulm_cleanup(void);

/*!
 * dclock function
 *
 * \return              Time (double)
 */
double ulm_dclock(void);

/*!
 * abort function
 *
 * \param comm          Communicator ID
 * \param exit code     Code with which to call exit()
 * \return              Does not return
 */
int _ulm_abort(int comm, int exitCode, char *file, int line);

#define ulm_abort(COMM, EXITCODE) \
    _ulm_abort((COMM), (EXITCODE), __FILE__, __LINE__)

/*
 * progress messaging
 */
int ulm_make_progress(void);

/*!
 * bind a point-to-point send message descriptor to a path
 * object
 *
 * \param message       request descriptor
 * \param message       Send message descriptor
 * \return              ULM return code
 */
int ulm_bind_pt2pt_message( void **SendDescriptor, int ctx, int destination);

/*!
 * set the functions to bind point-to-point messages to
 * path objects; which is by default ulm_bind_pt2pt_message ; 
 * these functions are not inherited by communicators derived 
 * from others
 *
 * \param comm                  Communicator ID
 * \param pt2ptFunction         Pointer to point-to-point message path
 *                              selection function
 * \return                      ULM return code
 */
int ulm_set_path_selection_functions(int comm, int (*pt2ptFunction) 
		(void **, int, int) );

/*!
 * get the functions that bind point-to-point and multicast messages
 * to path objects for a given communicator, which is by default
 * ulm_bind_pt2pt_message
 *
 * \param comm                  Communicator ID
 * \param pt2ptFunction         Pointer to pointer of point-to-point
 *                              message path selection function
 * \return                      ULM return code
 */
int ulm_get_path_selection_functions(int comm, int (**pt2ptFunction) 
		(void **, int, int) );

/*!
 * Get communicator specific topology pointer
 *
 * \param comm          Communicator
 * \param topology      On exit, pointer to the topology struct
 * \return              ULM return code
 */
int ulm_get_topology(int comm, ULMTopology_t ** topology);

/*!
 * Set communicator specific topology pointer
 *
 * \param comm          Communicator
 * \param topology      Pointer to a topology struct
 * \return              ULM return code
 */
int ulm_set_topology(int comm, ULMTopology_t *topology);

/*!
 * Test to see if there are messages pending in the queues of active
 * communicators
 *
 * \param flag          0 if no messages pending, 1 otherwise
 * \return              ULM return code
 */
int ulm_pending_messages(int *flag);

/*!
 * Non-blocking send
 *
 * \param buf           Data buffer
 * \param buf_size      Buffer size, in bytes, if dtype is NULL, or the
 *                      number of ULMType_t's in buf, if dtype is not NULL
 * \param dtype         Datatype descriptor for non-contiguous data,
 *                      NULL if data is contiguous
 * \param dest          Destination process
 * \param tag           Message tag
 * \param comm          Communicator ID
 * \param request       ULM request
 * \param sendMode      Send mode - standard, buffered, synchronous, or ready
 * \return              ULM return code
 */
int ulm_isend(void *buf, size_t size, ULMType_t *dtype,
      int dest, int tag, int comm, ULMRequestHandle_t *request, int sendMode);

/*!
 * Blocking send
 *
 * \param buf           Data buffer
 * \param buf_size      Buffer size, in bytes, if dtype is NULL, or the
 *                      number of ULMType_t's in buf, if dtype is not NULL
 * \param dtype         Datatype descriptor for non-contiguous data,
 *                      NULL if data is contiguous
 * \param dest          Destination process
 * \param tag           Message tag
 * \param comm          Communicator ID
 * \param sendMode      Send mode - standard, buffered, synchronous, or ready
 * \return              ULM return code
 */
int ulm_send(void *buf, size_t size, ULMType_t *dtype,
	     int dest, int tag, int comm, int sendMode);

/*!
 * Send multiple messages at once - this routine sets an upper limit
 * on the actual number of messages to be sent.
 *
 * \param buf           Array of send buffers
 * \param size          number of dtype elements
 * \param dtype         Datatype descriptors
 * \param source        ProcIDs of source process
 * \param tag           Tags for matching
 * \param comm          IDs of communicator
 * \param numToReceive  Number of receives to post and complets
 * \param status        Array of status objects
 * \return              ULM return code
 */
int ulm_send_vec(void **buf, size_t *size, ULMType_t ** dtype,
		 int *destinationProc, int *tag, int *comm,
		 int numToSend, ULMStatus_t *status);

/*!
 * Free request handle
 *
 * \param request       ULM request
 * \return              ULM return code
 */
int ulm_request_free(ULMRequestHandle_t *request);

/*!
 * Initialize send descriptor
 *
 * \param buf           Data buffer
 * \param size          Buffer size in bytes if dtype is NULL,
 *                      else number of datatype descriptors in buf
 * \param dtype         Datatype descriptor
 * \param dest          Destination process
 * \param tag           Message tag
 * \param comm          Communicator ID
 * \param request       ULM request
 * \param sendMode      Send mode - standard, buffered, synchronous, or ready
 * \param persistent    flag indicating if the request is persistent
 * \return              ULM return code
 */
int ulm_isend_init(void *buf, size_t size, ULMType_t *dtype, int dest,
                   int tag, int comm, ULMRequestHandle_t *request, int sendMode,
		   int persistent);

/*!
 * start request - either send or receive
 *
 * \param request       ULM request
 * \return              ULM return code
 */
int ulm_start(ULMRequestHandle_t *request);

/*!
 * Post non-blocking receive
 *
 * \param buf           Buffer for received buf
 * \param size          Size of buffer in bytes if dtype is NULL,
 *                      else number of datatype descriptors
 * \param dtype         Datatype descriptor
 * \param source        ProcID of source process
 * \param tag           Tag for matching
 * \param comm          ID of communicator
 * \param request       ULM request handle
 * \return              ULM return code
 */
int ulm_irecv(void *buf, size_t size, ULMType_t *dtype, int source,
	      int tag, int comm, ULMRequestHandle_t *request);

/*!
 * Post blocking receive
 *
 * \param buf           Buffer for received buf
 * \param size          Size of buffer in bytes if dtype is NULL,
 *                      else number of datatype descriptors
 * \param dtype         Datatype descriptor
 * \param source        ProcID of source process
 * \param tag           Tag for matching
 * \param comm          ID of communicator
 * \param status        ULM status handle
 * \return              ULM return code
 */
int ulm_recv(void *buf, size_t size, ULMType_t *dtype, int source,
	     int tag, int comm, ULMStatus_t *status);

/*!
 * Receive multiple messages at once - this routine sets an upper
 * limit on the actual number of messages to be received.
 *
 * \param buf           Array of receive buffers
 * \param size          Size of buffers in bytes if dtype is NULL,
 *                      else number of datatype descriptors
 * \param dtype         Datatype descriptors
 * \param source        ProcIDs of source process
 * \param tag           Tags for matching
 * \param comm          IDs of communicator
 * \param numToReceive  Number of receives to post and complets
 * \param status        Array of status objects
 * \return              ULM return code
 */
int ulm_recv_vec(void **buf, size_t *size, ULMType_t ** dtype,
		 int *sourceProc, int *tag, int *comm, int numToReceive,
		 ULMStatus_t *status);

/*!
 * Post non-blocking receive
 *
 * \param buf          Buffer for received buf
 * \param size         Size of buffer in bytes if dtype is NULL,
 *                     else number of datatype descriptors
 *\param dtype         Datatype descriptor
 *\param source        ProcID of source process
 *\param tag           Tag for matching
 *\param comm          ID of communicator
 *\param request       ULM request handle
 *\param persistent    flag indicating if the request is persistent
 *\return              ULM return code
 */
int ulm_irecv_init(void *buf, size_t size, ULMType_t *dtype, int source,
	      int tag, int comm, ULMRequestHandle_t *request, int persistent);

/*!
 * Wait until send/receive done
 *
 * \param request       ULM request
 * \param status        ULM status struct to be filled in
 * \return              ULM return code
 */
int ulm_wait(ULMRequestHandle_t *request, ULMStatus_t *status);

/*!
 * Test to see if send/recv done
 *
 * \param request       ULM request
 * \param status        ULM status struct to be filled in
 * \return              ULM return code
 */
int ulm_test(ULMRequestHandle_t *request, int *completed, ULMStatus_t *status);

/*!
 * Test posted request(s) for completion without blocking ulm_test
 * must be called to properly get status and deallocate request
 * objects
 *
 * \param requestArray  Array of requests to test for completion
 * \param numRequests   Number of requests in requestArray
 * \param completed     Flag to set if request is complete
 * \return              ULM return code
 */
int ulm_testall(ULMRequestHandle_t *requestArray, int numRequests, int *completed);

/*!
 * probe to see if there is a message that we can start to receive
 *
 * \param sourceProc    Source process
 * \param comm          Communicator ID
 * \param tag           Message tag
 * \param found         Flag to set true if a message to be received
 * \param statue        ULM status struct to be filled in
 * \return              ULM return code
 */
int ulm_iprobe(int sourceProc, int comm, int tag, int *found,
	       ULMStatus_t *status);

/*!
 * Is request persistent?
 *
 * \param request       ULM request handle
 * \return              1 if request is persistent, 0 otherwise
 */
int ulm_request_persistent(ULMRequestHandle_t request);

/*
 * Environment
 */

/*!
 * Get information about a specific context
 *
 * \param comm          Communicator ID
 * \param key           Keyword for the data of interest
 * \param buf           Buffer to hold the returned info
 * \param size          Size of buffer in bytes
 * \return              ULM return code
 *
 * Return the information specified by key for the given communicator.
 * If comm < 0, then return global information.
 *
 */
int ulm_get_info(int comm, ULMInfo_t key, void *buf, size_t size);


/*
 * Collective operations
 */

/*!
 *  ulm_allgather
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param recvcount     (int)       Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param comm          (int)       Communicator
 */
int ulm_allgather(void *sendbuf, int sendcount, ULMType_t *sendtype,
		  void *recvbuf, int recvcount, ULMType_t *recvtype,
		  int comm);

/*!
 * ulm_allgatherv
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer (
 * \param recvcounts    (int*)      Integer array (of length group size)
 * \param displs        (int*)      Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param comm          (int)       Communicator
 */
int ulm_allgatherv(void *sendbuf, int sendcount, ULMType_t *sendtype,
		   void *recvbuf, int *recvcounts, int *displs,
		   ULMType_t *recvtype, int comm);

/*!
 * ulm_allreduce
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param count         (integer)   Number of elements in the send buffer
 * \param type          (handle)    Data type of send buffer elements
 * \param OPtype        (handle)    OP type
 * \param comm          (int)       Communicator
 */
int ulm_allreduce(const void *sendbuf, void *recvbuf, int count,
		  ULMType_t *type, ULMOp_t *op, int comm);

/*!
 * ulm_allreduce_int (data type is int)
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param count         (integer)   Number of elements in the send buffer
 * \param OPtype        (handle)    OP type
 * \param comm          (int)       Communicator
 */
int ulm_allreduce_int(const void *sendbuf, void *recvbuf, int count,
		      ULMFunc_t *func, int comm);

/*!
 * ulm_alltoall
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer (
 * \param recvcounts    (int*)      Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param comm          (int)       Communicator
 */
int ulm_alltoall(void *sendbuf, int sendcount, ULMType_t *sendtype,
		 void *recvbuf, int recvcount, ULMType_t *recvtype,
		 int comm);

/*!
 * ulm_alltoallv
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (int*)      Array
 * \paran sdispls       (int*)
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer (
 * \param recvcounts    (int*)      Integer array (of length group size)
 * \param rdispls        int*)      Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param comm          (int)       Communicator
 */
int ulm_alltoallv(void *sendbuf, int *sendcount,
		  int *sdispls, ULMType_t *sendtype,
		  void *recvbuf, int *recvcount,
		  int *rdispls, ULMType_t *recvtype,
		  int comm);

/*!
 * ulm_barrier
 *
 * \param comm          (int)       Communicator
 */
int ulm_barrier(int comm);

/*!
 *  ulm_bcast
 * \param buf           (choice)    The starting address of the send buffer
 * \param count         (int)       Number of entries in the buffer
 * \param type          (handle)    Data type of send buffer elements
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 */
int ulm_bcast(void *buf, int count, ULMType_t *type, int root, int comm);

/*!
 * ulm_gather
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param recvcount     (int)       Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 */
int ulm_gather(void *sendbuf, int sendcount, ULMType_t *sendtype,
	       void *recvbuf, int recvcount, ULMType_t *recvtype,
	       int root, int comm);
/*!
 * ulm_gatherv
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer (
 * \param recvcounts    (int*)      Integer array (of length group size)
 * \param displs        (int*)      Integer array (of length group size)
 * \param recvtype      (handle)    Data type of receive buffer
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 */
int ulm_gatherv(void *sendbuf, int sendcount, ULMType_t *sendtype,
		void *recvbuf, int *recvcounts, int *displs,
		ULMType_t *recvtype, int root, int comm);

/*!
 * ulm_scatter
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param sendcount     (integer)   Number of elements in the send buffer
 * \param sendtype      (handle)    Data type of send buffer elements
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param recvcount     (int)       Number of elemetns to receive
 * \param recvtype      (handle)    Data type of receive buffer
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 */
int ulm_scatter(void *sendbuf, int sendcount, ULMType_t *sendtype,
		void *recvbuf, int recvcount, ULMType_t *recvtype,
		int root, int comm);

/*!
 * ulm_scatter
 *
 * \param sendbuf       (choice)     The starting address of the send buffer
 * \param sendcounts    (integer)    Integer array
 * \param sendtype      (handle)     Data type of send buffer elements
 * \param recvbuf       (choice)     Address of the receive buffer
 * \param recvcount     (int)        Number of elements to receive
 * \param recvtype      (handle)     Data type of receive buffer
 * \param root          (int)        rank of receiving process
 * \param comm          (int)        Communicator
 */
int ulm_scatterv(void *sendbuf, int *sendcounts, int *displs,
		 ULMType_t *sendtype,
		 void *recvbuf, int recvcount, ULMType_t *recvtype,
		 int root, int comm);


/*!
 * ulm_reduce
 *
 * \param sendbuf       (choice)    The starting address of the send buffer
 * \param recvbuf       (choice)    Address of the receive buffer
 * \param count         (integer)   Number of elements in the send buffer
 * \param type          (handle)    Data type of send buffer elements
 * \param OPtype        (handle)    OP type
 * \param root          (int)       rank of receiving process
 * \param comm          (int)       Communicator
 */
int ulm_reduce(const void *sendbuf, void *recvbuf, int count,
	       ULMType_t *type, ULMOp_t *op, int root, int comm);

int ulm_scan(const void *sendbuf, void *recvbuf, int count,
	     ULMType_t *type, ULMOp_t *op, int comm);

/*
 * group functions
 */
int ulm_group_size(int group, int *size);
int ulm_group_rank(int group, int *size);
int ulm_group_union(int group1Index, int group2Index, int *newGroupIndex);
int ulm_group_compare(int group1Index, int group2Index, int *result);
int ulm_group_intersection(int group1Index, int group2Index,
			   int *newGroupIndex);
int ulm_group_difference(int group1Index, int group2Index,
			 int *newGroupIndex);
int ulm_group_translate_ranks(int group1Index, int numRanks,
			      int *ranks1List, int group2Index,
			      int *ranks2List);
int ulm_group_incl(int group, int nRanks, int *ranks, int *newGroupIndex);
int ulm_group_excl(int group, int nRanks, int *ranks, int *newGroupIndex);
int ulm_group_range_incl(int group, int nTriplets, int ranges[][3],
			 int *newGroupIndex);
int ulm_group_range_excl(int group, int nTriplets, int ranges[][3],
			 int *newGroupIndex);
int ulm_group_free(int group);

/*
 * communicator manipulation functions
 */
int ulm_comm_group(int comm, int *group);
int ulm_comm_remote_group(int comm, int *group);

/*!
 * get number of processes in the local group for the given communicator
 *
 * \param comm          Communicator ID
 * \param size          On exit, contains the number of processes
 * \return              ULM/MPI return code
 */
int ulm_comm_size(int comm, int *size);
int ulm_comm_remote_size(int comm, int *size);

/*!
 * get the rank of the calling process in the local group for the
 * given communicator
 *
 * \param comm          Communicator ID
 * \param rank          On exit, contains the calling process's rank
 * \return              ULM/MPI return code
 */
int ulm_comm_rank(int comm, int *rank);

/*!
 *  compare communcator1 and communictor2
 *
 * \param comm1         Communicator ID
 * \param comm2         Communicator ID
 * \return              ULM/MPI return code
 */
int ulm_comm_compare(int comm1, int comm2, int *result);

/*!
 *  duplicate communicator
 *
 * \param inputComm     input communicator
 * \param outputComm    new outputed communicator
 * \return              ULM/MPI return code
 */
int ulm_comm_dup(int inputComm, int *outputComm);

/*!
 *  create new communicator from input group
 *
 * \param comm          input communicator
 * \param group         input group
 * \param newComm       new outputed communicator
 * \return              ULM/MPI return code
 */
int ulm_comm_create(int comm, int group, int *newComm);

/*!
 *  create multiple communicators by spliting a single communicator
 *
 * \param comm          input communicator
 * \param color         used as index to split comm
 * \param key           ordering index in color
 * \param newComm       new outputed communicator
 * \return              ULM/MPI return code
 */
int ulm_comm_split(int comm, int color, int key, int *newComm);

/*!
 *  test to see if the communicator is an inter-communicator
 *
 * \param comm          input communicator
 * \param isInterComm   result :: 1 - true , 0 - flase
 * \return              ULM/MPI return code
 */
int ulm_comm_test_inter(int comm, int *isInterComm);

/*!
 *  Set up intercommunicator
 *
 * \param localComm     local communicator
 * \param localLeader   rank of leader in local communicator
 * \param peerComm      comm that both leaders can use for communications
 * \param remoteLeader  rank of remote leader in peerComm
 * \param tag           "safe" tag for leader pt-2-pt communications
 * \param newInterComm  new inter-communicator
 * \return              ULM/MPI return code
 */
int ulm_intercomm_create(int localComm, int localLeader, int peerComm,
			 int remoteLeader, int tag, int *newInterComm);

/*!
 *  set up intra-communicator from an inter-communicator
 *
 * \param interComm     incoming inter-communicator communicator
 * \param high          my local group the "high" part of the new group
 * \param newIntraComm  new intra-communicator
 * \return              ULM/MPI return code
 */
int ulm_intercomm_merge(int interComm, int high, int *newIntraComm);

/*!
 * Set up a group of processes identified by the id comm.
 */
int ulm_communicator_alloc(int comm, int useThreads, int group1Index,
			   int group2Index, int setCtxCache,
			   int sizeCtxCache, int *lstCtxElements,
			   int commType, int okToDeleteComm,
			   int useSharedMem, int useSharedMemForColl);
/*!
 *  Try to release the resources associated with a communicator
 */
int ulm_comm_free(int comm);

/*!
 *  Release the resources associated with a communicator
 */
int ulm_communicator_really_free(int comm);

/*!
 * Barrier for all on-host processes in a communicator
 */
int ulm_barrier_intrahost(int comm);

/*!
 * Get a unique system tag (range) for the communicator
 *
 * \param comm          Communicator
 * \param count         Number of tags to allocate
 * \param tag           (Base) system tag
 * \return              ULM return code
 */
int ulm_get_system_tag(int comm, int count, int *tag);

/*!
 * Get/set index into error handler table
 *
 * \param comm          Communicator
 * \param index         Index into error handler table
 * \return              ULM return code
 */
int ulm_get_errhandler_index(int comm, int *index);
int ulm_set_errhandler_index(int comm, int index);

/*!
 * Lock/unlock a communicator for higher level libraries
 *
 * \param comm          Communicator
 * \return              ULM return code
 */
int ulm_comm_lock(int comm);
int ulm_comm_unlock(int comm);

/*
 * Attribute functions
 */

/*
 * c interface for creating new attribute data
 */
int ulm_c_keyval_create(MPI_Copy_function * copyFunction,
			MPI_Delete_function * deleteFunction,
			int *keyval, void *extraState);

/*
 * fortran interface for creating new attribute data
 */
int ulm_f_keyval_create(void *copyFunction,
			void *deleteFunction,
			int *keyval, void *extraState);

int ulm_attr_delete(int comm, int keyval);

int ulm_attr_get(int comm, int keyval, void *attributeVal, int *flag);

int ulm_attr_put(int comm, int keyval, void *attributeVal);

int ulm_keyval_free(int *keyval);

/*!
 * associative binary functions for reductions
 */
extern ULMFunc_t ulm_null;
extern ULMFunc_t ulm_cmax;
extern ULMFunc_t ulm_cmin;
extern ULMFunc_t ulm_csum;
extern ULMFunc_t ulm_cprod;
extern ULMFunc_t ulm_cmaxloc;
extern ULMFunc_t ulm_cminloc;
extern ULMFunc_t ulm_cbor;
extern ULMFunc_t ulm_cband;
extern ULMFunc_t ulm_cbxor;
extern ULMFunc_t ulm_clor;
extern ULMFunc_t ulm_cland;
extern ULMFunc_t ulm_clxor;
extern ULMFunc_t ulm_smax;
extern ULMFunc_t ulm_smin;
extern ULMFunc_t ulm_ssum;
extern ULMFunc_t ulm_sprod;
extern ULMFunc_t ulm_smaxloc;
extern ULMFunc_t ulm_sminloc;
extern ULMFunc_t ulm_sbor;
extern ULMFunc_t ulm_sband;
extern ULMFunc_t ulm_sbxor;
extern ULMFunc_t ulm_slor;
extern ULMFunc_t ulm_sland;
extern ULMFunc_t ulm_slxor;
extern ULMFunc_t ulm_imax;
extern ULMFunc_t ulm_imin;
extern ULMFunc_t ulm_isum;
extern ULMFunc_t ulm_iprod;
extern ULMFunc_t ulm_imaxloc;
extern ULMFunc_t ulm_iminloc;
extern ULMFunc_t ulm_ibor;
extern ULMFunc_t ulm_iband;
extern ULMFunc_t ulm_ibxor;
extern ULMFunc_t ulm_ilor;
extern ULMFunc_t ulm_iland;
extern ULMFunc_t ulm_ilxor;
extern ULMFunc_t ulm_lmax;
extern ULMFunc_t ulm_lmin;
extern ULMFunc_t ulm_lsum;
extern ULMFunc_t ulm_lprod;
extern ULMFunc_t ulm_lmaxloc;
extern ULMFunc_t ulm_lminloc;
extern ULMFunc_t ulm_lbor;
extern ULMFunc_t ulm_lband;
extern ULMFunc_t ulm_lbxor;
extern ULMFunc_t ulm_llor;
extern ULMFunc_t ulm_lland;
extern ULMFunc_t ulm_llxor;
extern ULMFunc_t ulm_ucmax;
extern ULMFunc_t ulm_ucmin;
extern ULMFunc_t ulm_ucsum;
extern ULMFunc_t ulm_ucprod;
extern ULMFunc_t ulm_ucmaxloc;
extern ULMFunc_t ulm_ucminloc;
extern ULMFunc_t ulm_ucbor;
extern ULMFunc_t ulm_ucband;
extern ULMFunc_t ulm_ucbxor;
extern ULMFunc_t ulm_uclor;
extern ULMFunc_t ulm_ucland;
extern ULMFunc_t ulm_uclxor;
extern ULMFunc_t ulm_usmax;
extern ULMFunc_t ulm_usmin;
extern ULMFunc_t ulm_ussum;
extern ULMFunc_t ulm_usprod;
extern ULMFunc_t ulm_usmaxloc;
extern ULMFunc_t ulm_usminloc;
extern ULMFunc_t ulm_usbor;
extern ULMFunc_t ulm_usband;
extern ULMFunc_t ulm_usbxor;
extern ULMFunc_t ulm_uslor;
extern ULMFunc_t ulm_usland;
extern ULMFunc_t ulm_uslxor;
extern ULMFunc_t ulm_uimax;
extern ULMFunc_t ulm_uimin;
extern ULMFunc_t ulm_uisum;
extern ULMFunc_t ulm_uiprod;
extern ULMFunc_t ulm_uimaxloc;
extern ULMFunc_t ulm_uiminloc;
extern ULMFunc_t ulm_uibor;
extern ULMFunc_t ulm_uiband;
extern ULMFunc_t ulm_uibxor;
extern ULMFunc_t ulm_uilor;
extern ULMFunc_t ulm_uiland;
extern ULMFunc_t ulm_uilxor;
extern ULMFunc_t ulm_ulmax;
extern ULMFunc_t ulm_ulmin;
extern ULMFunc_t ulm_ulsum;
extern ULMFunc_t ulm_ulprod;
extern ULMFunc_t ulm_ulmaxloc;
extern ULMFunc_t ulm_ulminloc;
extern ULMFunc_t ulm_ulbor;
extern ULMFunc_t ulm_ulband;
extern ULMFunc_t ulm_ulbxor;
extern ULMFunc_t ulm_ullor;
extern ULMFunc_t ulm_ulland;
extern ULMFunc_t ulm_ullxor;
extern ULMFunc_t ulm_fmax;
extern ULMFunc_t ulm_fmin;
extern ULMFunc_t ulm_fsum;
extern ULMFunc_t ulm_fprod;
extern ULMFunc_t ulm_fmaxloc;
extern ULMFunc_t ulm_fminloc;
extern ULMFunc_t ulm_dmax;
extern ULMFunc_t ulm_dmin;
extern ULMFunc_t ulm_dsum;
extern ULMFunc_t ulm_dprod;
extern ULMFunc_t ulm_dmaxloc;
extern ULMFunc_t ulm_dminloc;
extern ULMFunc_t ulm_qmax;
extern ULMFunc_t ulm_qmin;
extern ULMFunc_t ulm_qsum;
extern ULMFunc_t ulm_qprod;
extern ULMFunc_t ulm_qmaxloc;
extern ULMFunc_t ulm_qminloc;
extern ULMFunc_t ulm_scsum;
extern ULMFunc_t ulm_scprod;
extern ULMFunc_t ulm_dcsum;
extern ULMFunc_t ulm_dcprod;
extern ULMFunc_t ulm_qcsum;
extern ULMFunc_t ulm_qcprod;
extern ULMFunc_t ulm_ffmaxloc;
extern ULMFunc_t ulm_ddmaxloc;
extern ULMFunc_t ulm_qqmaxloc;
extern ULMFunc_t ulm_ffminloc;
extern ULMFunc_t ulm_ddminloc;
extern ULMFunc_t ulm_qqminloc;
extern ULMFunc_t ulm_llmax;
extern ULMFunc_t ulm_llmin;
extern ULMFunc_t ulm_llsum;
extern ULMFunc_t ulm_llprod;
extern ULMFunc_t ulm_llbor;
extern ULMFunc_t ulm_llband;
extern ULMFunc_t ulm_llbxor;
extern ULMFunc_t ulm_lllor;
extern ULMFunc_t ulm_llland;
extern ULMFunc_t ulm_lllxor;

/*
 * access functions to store needed data for MPI_Bsend between
 * persistent init and start calls
 */
void ulm_bsend_info_set(ULMRequestHandle_t request, void *originalBuffer,
		     size_t bufferSize, int count, ULMType_t *datatype);
void ulm_bsend_info_get(ULMRequestHandle_t request, void **originalBuffer,
		  size_t *bufferSize, int *count, ULMType_t ** datatype,
			int *comm);

/*
 * returns the status of the request object
 */
int ulm_request_status(ULMRequestHandle_t request);

/*
 * functions allowing users to set/get the current collective operations
 */
int ulm_get_collective_function(int key, void **op);
int ulm_set_collective_function(int key, void *op);

/*
 * argument checking functions
 */
int ulm_invalid_comm(int comm);
int ulm_invalid_request(ULMRequestHandle_t *request);
int ulm_invalid_source(int comm, int source);
int ulm_invalid_destination(int comm, int dest);
int ulm_presistant_request(ULMRequestHandle_t *request);
int ulm_am_i(int comm, int rank);

#ifdef __cplusplus
}
#endif

#endif /* !_ULM_H_ */
