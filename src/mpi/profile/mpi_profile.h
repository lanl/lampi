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



#ifndef _MPI_PROFILE_H_
#define _MPI_PROFILE_H_

enum {
    IRECV = 0,
    RECV = 1,
    ISEND = 2,
    SEND = 3,
    GATHER = 4,
    SCATTER = 5,
    REDUCE = 6,
    ALLGATHER = 7,
    ALLREDUCE = 8,
    ALLTOALL = 9,
    BCAST = 10
};

enum {
    MPI_MAX_P = 0,
    MPI_MIN_P = 1,
    MPI_SUM_P = 2,
    MPI_PROD_P = 3,
    MPI_MAXLOC_P = 4,
    MPI_MINLOC_P = 5,
    MPI_OTHER_OP_P = 6
};
typedef struct funcData {
    char name[8];
    int timesCalled;
    int bins[6];
    int op[7];
    int bytesSent;
    int bytesRecv;
    double cumTime;
} funcData;

typedef struct MPI_Stats {
    int myTask;
    int numberOfFunc;
    int e0;
    int e1;
    long long counter0;
    long long counter1;
    funcData funcInfo[11];
    double cumTime;
} MPI_Stats;
static char *funcList[11] =
{
    {"irecv"},
    {"recv"},
    {"isend"},
    {"send"},
    {"gather"},
    {"scatter"},
    {"reduce"},
    {"allgather"},
    {"allreduce"},
    {"alltoall"},
    {"bcast"}
};

static char *opList[10] =
{
    {"mpi_max"},
    {"mpi_min"},
    {"mpi_sum"},
    {"mpi_prod"},
    {"mpi_maxloc"},
    {"mpi_minloc"}
};

static MPI_Stats cumData;
static MPI_Stats procStats =
{
    0, 0, 0, 0, 0, 0,
    {
	{"IRECV", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},
	{"RECV", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},
	{"ISEND", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},
	{"SEND", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_Send
        {"GATHR", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_Gather
        {"SCATR", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_Scatter
        {"REDCE", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_Reduce
        {"ALGTR", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_AllGather
        {"ALRED", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_ALLReduce
        {"AL2AL", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0},	// MPI_AllToAll
        {"BCAST", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0}		// MPI_BCASTA
    }
};

int addToStats(int whichBin, int sizeOfSendData,
	       int sizeOfRecvData, int func, int op);
int addToStats_time(int whichBin, int sizeOfSendData,
		    int sizeOfRecvData, int func, int op,
		    double time);
int putIntoBins(int);
#endif				/* _MPI_PROFILE_H_ */
