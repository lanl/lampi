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



#ifndef _ULM_CONSTANTS_H_
#define _ULM_CONSTANTS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define ULM_REQUEST_NULL	0

enum {

    /* miscellaneous */
    ULM_ANY_PROC = -1,
    ULM_ANY_TAG = -1,
    ULM_THIS_PROCESS = -1,

    ULM_COMM_WORLD = 1,
    ULM_COMM_SELF = 2,

    /* send types */
    ULM_SEND_STANDARD = 1,
    ULM_SEND_BUFFERED = 2,
    ULM_SEND_SYNCHRONOUS = 3,
    ULM_SEND_READY = 4,

    /* status codes */
    ULM_STATUS_INVALID = 1,
    ULM_STATUS_INITED = 2,
    ULM_STATUS_INCOMPLETE = 3,
    ULM_STATUS_COMPLETE = 4,
    ULM_STATUS_INACTIVE = 5,

    /* group initialization parameters */
    ULM_THREAD_SAFE = 1,	/* set group for thread safety */
    ULM_THREAD_UNSAFE = 2,	/* set group not to consider thread safety */
    ULM_THREAD_MODE = 3,	/* use ULM global thread safety mode */

    /* Base value for unique tags used in collective communication */
    ULM_UNIQUE_BASE_TAG = -10000
};

/* Enumeration of info types that can be requested via ulm_get_info */

enum ULMInfo_t {
    ULM_INFO_PROCID,
    ULM_INFO_NUMBER_OF_PROCS,
    ULM_INFO_HOSTID,
    ULM_INFO_NUMBER_OF_HOSTS,
    ULM_INFO_ON_HOST_PROCID,
    ULM_INFO_ON_HOST_NUMBER_OF_PROCS,
    ULM_INFO_THIS_HOST_NUMBER_OF_PROCS,
    ULM_INFO_THIS_HOST_PROCIDS,
    ULM_INFO_THIS_HOST_COMM_ROOT,
    ULM_INFO_HOST_COMM_ROOTS,
    _ULM_INFO_END_
};
typedef enum ULMInfo_t ULMInfo_t;

/*
 * Keys for get/set operations on MPI collective functions
 */
enum {
    ULM_COLLECTIVE_ALLGATHER,
    ULM_COLLECTIVE_ALLGATHERV,
    ULM_COLLECTIVE_ALLREDUCE,
    ULM_COLLECTIVE_ALLTOALL,
    ULM_COLLECTIVE_ALLTOALLV,
    ULM_COLLECTIVE_ALLTOALLW,
    ULM_COLLECTIVE_BARRIER,
    ULM_COLLECTIVE_BCAST,
    ULM_COLLECTIVE_GATHER,
    ULM_COLLECTIVE_GATHERV,
    ULM_COLLECTIVE_REDUCE,
    ULM_COLLECTIVE_REDUCE_SCATTER,
    ULM_COLLECTIVE_SCAN,
    ULM_COLLECTIVE_SCATTER,
    ULM_COLLECTIVE_SCATTERV
};

/*
 * toggle hardware-specific collective use
 */
enum {
    ULM_HW_BARRIER = 0,
    ULM_HW_BCAST = 0,
    ULM_HW_REDUCE = 0
};

#ifdef __cplusplus
}
#endif

#endif  /* _ULM_CONSTANTS_H_ */
