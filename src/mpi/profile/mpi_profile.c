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

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include "mpi_profile.h"

#include "init/environ.h"

#define TIMER_INIT()    double s_time;\
                        double e_time;\
                        double diff_time;

#define TIMER(RC)       s_time = MPI_Wtime();\
                        if ((RC) != MPI_SUCCESS) fprintf(stderr,"error in func\n");\
                        e_time = MPI_Wtime();\
                        diff_time = e_time-s_time;

#define NUMFUNCS 10

#define DEBUG 1
TIMER_INIT();
int HWCounters = 0;
int MPI_Init(int *argc, char ***argv)
{
    char *val;
    int result;
    int mytask;
    int gen_start;
    int gen_read;
    long long c0, c1;


    MPI_Comm_rank(MPI_COMM_WORLD, &mytask);
    procStats.myTask = mytask;
    procStats.numberOfFunc = NUMFUNCS;
    fprintf(stderr, "MPI Profiling Interface\n");
    return result;
}

//      The collective Functions

int MPI_Allgather(void *sendbuf, int sendcount, MPI_Datatype senddatatype,
                  void *recvbuf, int recvcount, MPI_Datatype recvdatatype,
                  MPI_Comm comm)
{
    int r_extent;
    int result;
    int retCode;
    int s_extent;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    TIMER(PMPI_Allgather(sendbuf, sendcount, senddatatype, recvbuf, recvcount,
                         recvdatatype, comm));

    MPI_Type_size(senddatatype, &s_extent);
    MPI_Type_size(recvdatatype, &r_extent);
    sizeOfSendData = sendcount * s_extent;
    sizeOfRecvData = recvcount * r_extent;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats_timer(whichBin, sizeOfSendData, sizeOfRecvData, ALLGATHER, 0, diff_time);

    return result;
}

int MPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype senddatatype,
		 void *recvbuf, int recvcount, MPI_Datatype recvdatatype,
		 MPI_Comm comm)
{
    int r_extent;
    int rc;
    int result;
    int retCode;
    int s_extent;
    int sizeOfRecvData;
    int sizeOfSendData;
    int tag;
    int whichBin;
    TIMER(PMPI_Alltoall(sendbuf, sendcount, senddatatype, recvbuf, recvcount,
			recvdatatype, comm));

    MPI_Type_size(senddatatype, &s_extent);
    MPI_Type_size(recvdatatype, &r_extent);
    sizeOfSendData = sendcount * s_extent;
    sizeOfRecvData = recvcount * r_extent;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats_timer(whichBin, sizeOfSendData, sizeOfRecvData, ALLTOALL, 0, diff_time);
    return result;
}

int MPI_Allreduce(void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype mtype, MPI_Op op, MPI_Comm comm)
{
    int Op;
    int extent;
    int result;
    int retCode;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    TIMER(PMPI_Allreduce(sendbuf, recvbuf, count, mtype, op, comm));
    MPI_Type_size(mtype, &extent);
    sizeOfSendData = count * extent;
    sizeOfRecvData = count * extent;
    whichBin = putIntoBins(sizeOfSendData);
    Op = whichOp(op);
    retCode = addToStats_timer(whichBin, sizeOfSendData, sizeOfRecvData, Op, ALLREDUCE, diff_time);
    return result;
}

int MPI_Bcast(void *buffer, int count, MPI_Datatype type,
	      int root, MPI_Comm comm)
{
    int result;
    int retCode;
    int s_extent;
    int sizeOfData;
    int whichBin;
    MPI_Type_size(type, &s_extent);
    sizeOfData = count * s_extent;
    TIMER(PMPI_Bcast(buffer, count, type, root, comm));
    whichBin = putInfoBins(sizeOfData);
    retCode = addToStats_timer(whichBin, sizeOfData, sizeOfRecvData, Op,BCAST, diff_time);
}

int MPI_Gather(void *sendbuf, int sendcount, MPI_Datatype senddatatype,
	       void *recvbuf, int recvcount, MPI_Datatype recvdatatype,
	       int root, MPI_Comm comm)
{
    int r_extent;
    int result;
    int retCode;
    int s_extent;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    TIMER(PMPI_Gather(sendbuf, sendcount, senddatatype, recvbuf, recvcount,
		      recvdatatype, root, comm));

    MPI_Type_size(senddatatype, &s_extent);
    MPI_Type_size(recvdatatype, &r_extent);
    sizeOfSendData = sendcount * s_extent;
    sizeOfRecvData = recvcount * r_extent;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats_timer(whichBin, sizeOfSendData, sizeOfRecvData, GATHER, 0, diff_time);

    return result;
}

int MPI_Reduce(void *sendbuf, void *recvbuf, int count,
               MPI_Datatype mtype, MPI_Op op, int root, MPI_Comm comm)
{
    int Op;
    int extent;
    int result;
    int retCode;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    TIMER(PMPI_Reduce(sendbuf, recvbuf, count, mtype, op, root, comm));
    MPI_Type_size(mtype, &extent);
    sizeOfSendData = count * extent;
    sizeOfRecvData = count * extent;
    whichBin = putIntoBins(sizeOfSendData);
    Op = whichOp(op);
    retCode = addToStats_timer(whichBin, sizeOfSendData, sizeOfRecvData, Op, REDUCE, diff_time);
    return result;
}

int MPI_Scatter(void *sendbuf, int sendcount, MPI_Datatype senddatatype,
		void *recvbuf, int recvcount, MPI_Datatype recvdatatype,
		int root, MPI_Comm comm)
{
    int r_extent;
    int result;
    int retCode;
    int s_extent;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    TIMER(MPI_Gather(sendbuf, sendcount, senddatatype, recvbuf, recvcount,
		     recvdatatype, root, comm));

    MPI_Type_size(senddatatype, &s_extent);
    MPI_Type_size(recvdatatype, &r_extent);
    sizeOfSendData = sendcount * s_extent;
    sizeOfRecvData = recvcount * r_extent;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats(whichBin, sizeOfSendData, sizeOfRecvData, 0, SCATTER);

    return result;
}

int MPI_Send(void *buffer, int count, MPI_Datatype datatype,
	     int dest, int tag, MPI_Comm comm)
{
    int extent;
    int sizeOfSendData;
    int sizeOfRecvData;
    int whichBin;
    int retCode;
    int result;
    result = PMPI_Send(buffer, count, datatype, dest, tag, comm);
    MPI_Type_size(datatype, &extent);
    sizeOfSendData = count * extent;
    sizeOfRecvData = 0;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats(whichBin, sizeOfSendData, sizeOfRecvData, 0, SEND);

    return result;
}


int MPI_Isend(void *buffer, int count, MPI_Datatype datatype,
	      int dest, int tag, MPI_Comm comm, MPI_Request * request)
{
    int extent;
    int result;
    int retCode;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;

    result = PMPI_Isend(buffer, count, datatype, dest, tag, comm, request);
    MPI_Type_size(datatype, &extent);
    sizeOfSendData = count * extent;
    sizeOfRecvData = 0;
    whichBin = putIntoBins(sizeOfSendData);
    retCode = addToStats(whichBin, sizeOfSendData, sizeOfRecvData, 0, ISEND);

    return result;
}

int MPI_Recv(void *buffer, int count, MPI_Datatype datatype,
	     int dest, int tag, MPI_Comm comm, MPI_Status * status)
{
    int extent;
    int result;
    int retCode;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    result = PMPI_Recv(buffer, count, datatype, dest, tag, comm, status);
    MPI_Type_size(datatype, &extent);
    sizeOfRecvData = count * extent;
    sizeOfSendData = 0;
    whichBin = putIntoBins(sizeOfRecvData);
    retCode = addToStats(whichBin, sizeOfSendData, sizeOfRecvData, 0, RECV);
 
    return result;
}


int MPI_Irecv(void *buffer, int count, MPI_Datatype datatype,
	      int dest, int tag, MPI_Comm comm, MPI_Request * request)
{
    int extent;
    int sizeOfRecvData;
    int sizeOfSendData;
    int whichBin;
    int result;
    int retCode;
    double s_time;
    double e_time;
    double diff_time;

    s_time = MPI_Wtime();
    result = PMPI_Irecv(buffer, count, datatype, dest, tag, comm, request);
    e_time = MPI_Wtime();
    diff_time = e_time - s_time;
    MPI_Type_size(datatype, &extent);
    sizeOfRecvData = count * extent;
    sizeOfSendData = 0;
    whichBin = putIntoBins(sizeOfRecvData);
    retCode = addToStats(whichBin, sizeOfSendData, sizeOfRecvData, 0, IRECV);
    return result;
}


int whichOp(MPI_Op op)
{
    if (op == MPI_MAX)
	return MPI_MAX_P;
    if (op == MPI_MIN)
	return MPI_MIN_P;
    if (op == MPI_SUM)
	return MPI_SUM_P;
    if (op == MPI_PROD)
	return MPI_PROD_P;
    if (op == MPI_MAXLOC)
	return MPI_MAXLOC_P;
    if (op == MPI_MINLOC)
	return MPI_MINLOC_P;

    return MPI_OTHER_OP_P;
}


int putIntoBins(int sizeOfData)
{
    int whichBin = 0;
    if (sizeOfData > 101 && sizeOfData < 1000)
	whichBin = 1;
    if (sizeOfData > 1001 && sizeOfData < 10000)
	whichBin = 2;
    if (sizeOfData > 10001 && sizeOfData < 100000)
	whichBin = 3;
    if (sizeOfData > 100001)
	whichBin = 4;
    return whichBin;
}

int addToStats(int whichBin, int sizeOfSendData, int sizeOfRecvData, int Op, int func)
{
    // 3 funcs require us to count the
    //  number and type of operations (MPI_MAX,MPI_MIN)
    procStats.funcInfo[func].bins[whichBin]++;
    if (func == REDUCE || func == ALLREDUCE)
	procStats.funcInfo[func].op[Op]++;
    procStats.funcInfo[func].bytesRecv += sizeOfRecvData;
    procStats.funcInfo[func].bytesSent += sizeOfSendData;
    procStats.funcInfo[func].timesCalled++;
    procStats.funcInfo[func].cumTime = 0;
    return 0;
}

int addToStats_timer(int whichBin, int sizeOfSendData, int sizeOfRecvData, int Op, int func, double time)
{
    procStats.funcInfo[func].bins[whichBin]++;
    if (func == REDUCE || func == ALLREDUCE)
	procStats.funcInfo[func].op[Op]++;
    procStats.funcInfo[func].bytesRecv += sizeOfRecvData;
    procStats.funcInfo[func].bytesSent += sizeOfSendData;
    procStats.funcInfo[func].timesCalled++;
    procStats.funcInfo[func].cumTime += time;
    return 0;
}

int MPI_Finalize()
{
    char 	*rootFile = "ulmProfile.";
    char 	fileName[14];
    //char *val;
    FILE 	*fp;
    int 	numProcs = 0;
    int 	i = 0;
    int 	sizeOfInfo = 0;
    int 	gen_start = 0;
    int 	gen_read = 0;
    int 	e0 = 0;
    int 	e1 = 0;
    long long c0 = 0;
    long long c1 = 0;
    size_t 	nitems = 1;


    if (HWCounters == 0) {
	if ( lampi_environ_find_integer("T5_EVENT0", &e0) )
	    e0 = E0;

	if ( lampi_environ_find_integer("T5_EVENT0", &e1) )
	    e1 = E1;


	if ((gen_read = read_counters(e0, &c0, e1, &c1)) < 0) {
	    perror("read_counters");
	}
	print_counters(e0, c0, e1, c1);
	//
	procStats.e0 = e0;
	procStats.e1 = e1;
	procStats.counter0 = c0;
	procStats.counter1 = c1;
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
    sprintf(fileName, "%s%.3i", rootFile, procStats.myTask);
    fp = fopen(fileName, "w");
    if (fp == NULL) {
	fprintf(stderr, "Unable to open the profile.data\n");
	goto FINALIZE;
    }
    fwrite((void *) &procStats, sizeof(MPI_Stats), nitems, fp);
    fclose(fp);
FINALIZE:PMPI_Finalize();
    return 0;
}
