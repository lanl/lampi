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
