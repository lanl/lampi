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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "internal/mpi.h"
#include "Queues/Communicator.h"
#include "Queues/globals.h"
#include "Utility/Utility.h"
#include "mem/ULMMallocMacros.h"
#include "collective/coll_fns.h"
#include "Utility/inline_copy_functions.h"
#include "internal/type_copy.h"


extern "C" int ulm_scatter(void *sendbuf,
			   int sendcount,
			   ULMType_t * sendtype,
			   void *recvbuf,
			   int recvcount,
			   ULMType_t * recvtype, int root, int comm)
{
    int i;
    int rc;
    int self;
    int nprocs;
    int nhost;
    int root_of_host;
    int host_of_root;
    long long tag;
    int hostid;
    int tmp_recv;
    void *ptr;
    void *tmpbuf;
    void *RESTRICT_MACRO shared_buffer;
    size_t shared_buffer_size = 0;
    Communicator *comm_ptr;
    Group *group;

    ptr = NULL;
    tag = 0;
    comm_ptr = (Communicator *) communicators[comm];
    group = comm_ptr->localGroup;
    rc = ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_PROCS, &nprocs,
		      sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS, &nhost, sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_HOSTID, &hostid, sizeof(int));
    rc = ulm_get_info(comm, ULM_INFO_THIS_HOST_COMM_ROOT, &root_of_host,
		      sizeof(int));
    host_of_root =
	group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];

    if (hostid == host_of_root)
	group->hostIndexInGroup = root;

    // if the hd bcast works as advertised than
    // no really need for any shared memory tricks
    tmp_recv = recvcount * recvtype->extent;
    if (self == root) {
	rc = ulm_bcast_quadrics(sendbuf, nprocs * sendcount, sendtype,
				 root, comm);
	ptr = (unsigned char *) sendbuf + self * tmp_recv;
	coll_desc->flag = 2;
    } else {
	rc = ulm_bcast_quadrics(shared_buffer, nprocs * recvcount,
				 recvtype, root, comm);
	ptr = (unsigned char *) shared_buffer + self * tmp_recv;
    }
    memcpy(recvbuf, ptr, (size_t) tmp_recv);
    return 0;
}
