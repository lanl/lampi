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

/*
 * Initialization
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(HAVE_PTY_H)
#  include <pty.h>      /* Normal location of openpty() */
#elif defined(HAVE_UTIL_H)
#  include <util.h>     /* BSD location of openpty() */
#else
#  undef ENABLE_PTY_STDIO
#  define ENABLE_PTY_STDIO 0
#  define openpty(A,B,C,D,E) -1
#endif

#include "queue/Communicator.h"
#include "queue/contextID.h"
#include "queue/globals.h"
#include "client/adminMessage.h"
#include "client/daemon.h"
#include "util/Lock.h"
#include "util/MemFunctions.h"
#include "util/Utility.h"
#include "util/dclock.h"
#include "util/if.h"
#include "util/misc.h"
#include "init/environ.h"
#include "init/fork_many.h"
#include "init/init.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/system.h"
#include "mem/FreeLists.h"
#include "mpi/constants.h"
#include "path/common/BaseDesc.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/pathContainer.h"
#include "os/numa.h"
#include "ulm/ulm.h"

#if ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/sharedmem/SMPfns.h"
#endif /* SHARED_MEMORY */

#if ENABLE_NUMA && defined(__mips)
#include "os/IRIX/SN0/acquire.h"
bool useRsrcAffinity = true;
bool useDfltAffinity = true;
bool affinMandatory = true;
int nCpPNode = 2;
#endif /* ENABLE_NUMA and __mips */

#define ULM_INSTANTIATE_GLOBALS
#include "path/common/pathGlobals.h"
#undef ULM_INSTANTIATE_GLOBALS

/*
 * File scope variables
 */

static int initialized = 0;
static int stdin_parent;
static int stdin_child;
static int *stdout_parent;
static int *stdout_child;
static int *stderr_parent;
static int *stderr_child;

/*
 * inline functions
 */
void lampi_init_print(const char *string)
{
    if (0) {
        FILE *fp = fopen("/tmp/ddd/log", "a+");
        fprintf(fp, "%s\n", string);
        fflush(fp);
        fclose(fp);
    }

    fprintf(stderr, "LA-MPI: *** %s\n", string);
}


/*
 * Entry point for LA-MPI initialization.
 *
 * Any error here is fatal so there is no return code.
 */
void lampi_init(void)
{
    if (initialized) {
        return;
    }
    initialized = 1;

    lampi_init_prefork_initialize_state_information(&lampiState);
    lampi_init_prefork_environment(&lampiState);
    lampi_init_prefork_process_resources(&lampiState);
    lampi_init_prefork_globals(&lampiState);
    lampi_init_prefork_resource_management(&lampiState);
    lampi_init_prefork_check_stdio(&lampiState);
    lampi_init_prefork_connect_to_mpirun(&lampiState);
    lampi_init_prefork_receive_setup_params(&lampiState);
    lampi_init_prefork_ip_addresses(&lampiState);
    lampi_init_prefork_debugger(&lampiState);
    lampi_init_prefork_resources(&lampiState);
    lampi_init_prefork_paths(&lampiState);
    lampi_init_prefork_stdio(&lampiState);

    lampi_init_fork(&lampiState);     /* all la-mpi procs created here */

    lampi_init_postfork_pids(&lampiState);
    lampi_init_postfork_debugger(&lampiState);
    lampi_init_postfork_stdio(&lampiState);
    lampi_init_postfork_resource_management(&lampiState);
    lampi_init_postfork_globals(&lampiState);
    lampi_init_postfork_resources(&lampiState);
    lampi_init_postfork_ip_addresses(&lampiState);    /* exchange IP addresses */
    lampi_init_postfork_paths(&lampiState);
    lampi_init_postfork_communicators(&lampiState);
#ifdef USE_ELAN_COLL
    lampi_init_postfork_coll_setup(&lampiState);      /* enable hw/bcast support */
#endif

    lampi_init_wait_for_start_message(&lampiState);   /* barrier on all procs */
    if (lampiState.verbose) {
        fprintf(stderr, "LA-MPI: *** Process %ld is rank %d\n",
                (long) getpid(), lampiState.global_rank);
    }
    lampi_init_check_for_error(&lampiState);

    /* daemon process goes into loop */
    if (lampiState.iAmDaemon) {
        if (lampiState.verbose) {
            fprintf(stderr, "LA-MPI: *** Daemon initialized (stderr)\n");
            fprintf(stdout, "LA-MPI: *** Daemon initialized (stdout)\n");
            fflush(stdout);
        }
        lampi_daemon_loop(&lampiState);
    } else if (lampiState.global_rank == 0) {
        if (lampiState.quiet == 0) {
            if (ENABLE_DEBUG) {
                fprintf(stderr,
                        "LA-MPI: *** libmpi (" PACKAGE_VERSION "-debug)\n");
            } else {
                fprintf(stderr, "LA-MPI: *** libmpi (" PACKAGE_VERSION ")\n");
            }
            fprintf(stderr, "LA-MPI: *** Copyright 2001-2004, ACL, "
                    "Los Alamos National Laboratory\n");
        }
    }
}


void lampi_init_check_for_error(lampiState_t *s)
{
    struct {
        int error;
        char *string;
    } lookup[] = {
        { ERROR_LAMPI_INIT_PREFORK,
          "Initialization failed before fork" },
        { ERROR_LAMPI_INIT_POSTFORK,
          "Initialization failed after fork" },
        { ERROR_LAMPI_INIT_ALLFORKED,
          "Initialization failed after all processes forked" },
        { ERROR_LAMPI_INIT_PREFORK_INITIALIZING_GLOBALS,
          "Initialization failed initializing globals" },
        { ERROR_LAMPI_INIT_PREFORK_INITIALIZING_PROCESS_RESOURCES,
          "Initialization failed initializing process resources" },
        { ERROR_LAMPI_INIT_PREFORK_INITIALIZING_STATE_INFORMATION,
          "Initialization failed initializing state" },
        { ERROR_LAMPI_INIT_SETTING_UP_CORE,
          "Initialization failed setting up core file handling" },
        { ERROR_LAMPI_INIT_PREFORK_RESOURCE_MANAGEMENT,
          "Initialization failed setting up resource management before fork" },
        { ERROR_LAMPI_INIT_POSTFORK_RESOURCE_MANAGEMENT,
          "Initialization failed setting up resource management after fork" },
        { ERROR_LAMPI_INIT_ALLFORKED_RESOURCE_MANAGEMENT,
          "Initialization failed setting up resource management after all forked" },
        { ERROR_LAMPI_INIT_AT_FORK,
          "Initialization failed at fork" },
        { ERROR_LAMPI_INIT_CONNECT_TO_DAEMON,
          "Initialization failed connecting to daemon" },
        { ERROR_LAMPI_INIT_CONNECT_TO_MPIRUN,
          "Initialization failed connecting to mpirun" },
        { ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS,
          "Initialization failed receiving run-time parameters" },
        { ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY,
          "Initialization failed receiving run-time parameters for shared memory path" },
        { ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS,
          "Initialization failed receiving run-time parameters for Quadrics path" },
        { ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_GM,
          "Initialization failed receiving run-time parameters for Myrinet GM path" },
        { ERROR_LAMPI_INIT_PREFORK_STDIO,
          "Initialization failed setting up stdio before fork" },
        { ERROR_LAMPI_INIT_POSTFORK_STDIO,
          "Initialization failed setting up stdio after fork" },
        { ERROR_LAMPI_INIT_PREFORK_PATHS,
          "Initialization failed setting up paths before fork" },
        { ERROR_LAMPI_INIT_POSTFORK_PATHS,
          "Initialization failed setting up paths after fork" },
        { ERROR_LAMPI_INIT_ALLFORKED_PATHS,
          "Initialization failed setting up paths after all forked" },
        { ERROR_LAMPI_INIT_PREFORK_UDP,
          "Initialization failed setting up UDP path before fork" },
        { ERROR_LAMPI_INIT_POSTFORK_UDP,
          "Initialization failed setting up UDP path after fork" },
        { ERROR_LAMPI_INIT_PREFORK_RMS,
          "Initialization failed setting up RMS before fork" },
        { ERROR_LAMPI_INIT_POSTFORK_RMS,
          "Initialization failed setting up RMS after fork" },
        { ERROR_LAMPI_INIT_POSTFORK_GM,
          "Initialization failed setting up GM after fork" },
        { ERROR_LAMPI_INIT_POSTFORK_IB,
          "Initialization failed setting up IB after fork" },
        { ERROR_LAMPI_INIT_POSTFORK_PIDS,
          "Initialization failed reporting application PIDs after fork" },
        { ERROR_LAMPI_INIT_PATH_CONTAINER,
          "Initialization failed setting up path in global path container" },
        { ERROR_LAMPI_INIT_WAIT_FOR_START_MESSAGE,
          "Initialization failed waiting for start message" },
        { ERROR_LAMPI_INIT_MAX,
          NULL }
    };
    int i;

    if (s->error == LAMPI_INIT_SUCCESS) {
        return;
    } else {
        for (i = 0; i < ERROR_LAMPI_INIT_MAX; i++) {
            if (s->error == lookup[i].error) {
                fprintf(stderr, "LA-MPI: lampi_init: Error: %s\n", lookup[i].string);
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
        }
        fprintf(stderr, "LA-MPI: lampi_init: Unknown error\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
}


void lampi_init_prefork_environment(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_environment");
    }

    lampi_environ_init();
    lampi_environ_find_integer("LAMPI_VERBOSE", &(s->verbose));
    if (s->verbose) {
        lampi_init_print("lampi_init");
        lampi_init_print("lampi_init_prefork_environment");
        lampi_environ_dump();
    }
}


void lampi_init_prefork_globals(lampiState_t *s)
{
    ssize_t initialAllocation;
    ssize_t minAllocationSize;
    int nPools;
    int nArrayElementsToAdd;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_globals");
    }

    bytesPerProcess = 500 * getpagesize();
    largePoolBytesPerProcess = 16384 * getpagesize();

    /*
     * setup shared memory pool to be used before the fork for
     * persistent objects that must reside in shared memory
     */
    initialAllocation = 0;
    minAllocationSize = 1000 * getpagesize();
    nPools = 1;
    nArrayElementsToAdd = 10;
    SharedMemoryPools.init(initialAllocation, minAllocationSize, nPools,
                           nArrayElementsToAdd, false);

    /* do not prepend informative prefix to stdout/stderr */
    lampiState.output_prefix = 0;
    lampiState.quiet = 0;
    lampiState.map_global_proc_to_on_host_proc_id = 0;
}


void lampi_init_postfork_globals(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_globals");
    }

    lampiState.local_rank = lampiState.memLocalityIndex = s->local_rank;
    if (s->client) {
        s->client->setLocalProcessRank(s->local_rank);
    }
}


void lampi_init_prefork_resource_management(lampiState_t *s)
{
    int value;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_resource_management");
    }

    /* initialize data */
#if ENABLE_BPROC
    /* send current node id instead of doing scan of addresses on master */
    s->hostid = bproc_currnode();
#else
    s->hostid = adminMessage::UNKNOWN_HOST_ID;
#endif
    s->local_size = adminMessage::UNKNOWN_N_PROCS;

    /* check to see if RMS is in use */
    value = lampi_environ_find_integer("RMS_NNODES", &(s->nhosts));
    if (value == LAMPI_ENV_ERR_NOT_FOUND) {
        s->error = ERROR_LAMPI_INIT_PREFORK_RESOURCE_MANAGEMENT;
        return;
    }

    if (s->nhosts > 0) {
        s->interceptSTDio = 0;
        lampi_init_prefork_rms(s);
    }
}


void lampi_init_postfork_resource_management(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_resource_management");
    }
    if (s->quadrics) {
        lampi_init_postfork_rms(s);
    }
}


void lampi_init_allforked_resource_management(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_allforked_resource_management");
    }

    lampi_init_allforked_rms(s);
}


void lampi_init_prefork_resources(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_resources");
    }

    setupMemoryPools();
    setupPerProcSharedMemPools(s->local_size);
    InitSendDescriptors(s->local_size);

    /* initialize resources needed for administrative collective
     *   operations */
    int daemonIsHostCommRoot = 0;
    if (s->useDaemon)
        daemonIsHostCommRoot = 1;

    ssize_t lenSMBuffer = adminMessage::MINCOLLMEMPERPROC *
        s->client->totalNumberOfProcesses();
    void *sharedBufferPtr =
        SharedMemoryPools.getMemorySegment(lenSMBuffer, SMPPAGESIZE);
    void *lockPtr = (Locks *) SharedMemoryPools.getMemorySegment
        (sizeof(Locks), CACHE_ALIGNMENT);
    void *CounterPtr = (long long *) SharedMemoryPools.getMemorySegment
        (sizeof(long long), CACHE_ALIGNMENT);
    int *sPtr = (int *) SharedMemoryPools.getMemorySegment
        (sizeof(int), CACHE_ALIGNMENT);
    s->client->setupCollectives(s->local_rank, s->hostid,
                                s->map_global_rank_to_host,
                                daemonIsHostCommRoot, lenSMBuffer,
                                sharedBufferPtr, lockPtr, CounterPtr,
                                sPtr);
#if ENABLE_SHARED_MEMORY
    // setup queues for on host message fragements
    SetUpSharedMemoryQueues(local_nprocs());
#endif                          // SHARED_MEMORY

    // List of unprocessed frags for collective operations - resides in
    // shared memory
    _ulm_CommunicatorRecvFrags =
        (SharedMemDblLinkList **) SharedMemoryPools.
        getMemorySegment(sizeof(DoubleLinkList *) * local_nprocs(),
                         CACHE_ALIGNMENT);
    if (!_ulm_CommunicatorRecvFrags) {
        ulm_exit(("Unable to allocate space for _ulm_CommunicatorRecvFrags\n"));
    }
    // allocate individual lists and initialize locks
    for (int comm_rt = 0; comm_rt < local_nprocs(); comm_rt++) {
        // allocate shared memory
        _ulm_CommunicatorRecvFrags[comm_rt] = (SharedMemDblLinkList *)
            SharedMemoryPools.getMemorySegment(sizeof(DoubleLinkList),
                                               CACHE_ALIGNMENT);
        if (!(_ulm_CommunicatorRecvFrags[comm_rt])) {
            ulm_exit(("Unable to allocate space for "
                      "_ulm_CommunicatorRecvFrags[%d]\n", comm_rt));
        }
        // run the constuctor
        new(_ulm_CommunicatorRecvFrags[comm_rt]) SharedMemDblLinkList;

        // initialize locks
        _ulm_CommunicatorRecvFrags[comm_rt]->Lock.init();
    }

    //
    // create pool of processor group objects
    //
    // sanity check
    if (maxCommunicatorInstances <= 0) {
        ulm_exit(("Unable to instantiate communicators\n"
                  "Upper limit :: %d\n", maxCommunicatorInstances));
    }
    //
    // initialize pool for SW SMP barriers
    //
    int retVal = allocSWSMPBarrierPools();
    if (retVal != ULM_SUCCESS) {
        ulm_exit(("Error: allocating resources for SW SMP barrier pool.\n"));
    }

#if ENABLE_RELIABILITY
    // allocate info structure for reliable retransmission, etc.
    reliabilityInfo = (ReliabilityInfo *) SharedMemoryPools.
        getMemorySegment(sizeof(ReliabilityInfo), CACHE_ALIGNMENT);
    if (!reliabilityInfo) {
        ulm_exit(("Error: allocating resources for reliability information.\n"));
    }
    new(reliabilityInfo) ReliabilityInfo;
#endif

    //
    //  Setting up the Shared Memory for collectives operations
    //    CollectiveSMBufferPool_t is defined in the  SharedMemForCollective.h
    //
    SharedMemForCollective =
        (CollectiveSMBufferPool_t *) SharedMemoryPools.
        getMemorySegment(sizeof(CollectiveSMBufferPool_t),
                         CACHE_ALIGNMENT);

    if (!SharedMemForCollective) {
        ulm_exit(("Unable to allocate space for SharedMemForCollective *\n"));
    }
    /*
     * Initialize the CollectiveSMBufferPool_t structure
     */
    /* compute the size of shared memory segement */
    size_t collectiveMem = MIN_COLL_MEM_PER_PROC * nprocs();
    if (collectiveMem < 4 * SMPPAGESIZE)
        collectiveMem = 4 * SMPPAGESIZE;
    if (collectiveMem < MIN_COLL_SHARED_WORKAREA)
        collectiveMem = MIN_COLL_SHARED_WORKAREA;
    /* compute size for controlFlags array - flagElement_t is padded orrectly */
    size_t controlFlagsSize = local_nprocs() * sizeof(flagElement_t);
    SharedMemForCollective->lock.init();
    for (int ele = 0; ele < MAX_COMMUNICATOR_SHARED_OBJECTS; ele++) {
        SharedMemForCollective->collCtlData[ele].inUse = 0;
        SharedMemForCollective->collCtlData[ele].contextID = -1;
        SharedMemForCollective->collCtlData[ele].localCommRoot = -1;
        SharedMemForCollective->collCtlData[ele].nFreed = 0;
        for (int desc = 0; desc < N_COLLCTL_PER_COMM; desc++) {
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                allocRefCount = 0;
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                freeRefCount = -1;
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                lock.init();
            /* allocate the shared memory segment */
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                max_length = collectiveMem;
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                mem = (void *)
                SharedMemoryPools.getMemorySegment(collectiveMem,
                                                   CACHE_ALIGNMENT);
            if (!SharedMemForCollective->collCtlData[ele].
                collDescriptor[desc].mem) {
                ulm_exit(("Unable to allocate space for collDescriptor memory\n"));
            }
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                controlFlags = (flagElement_t *)
                SharedMemoryPools.getMemorySegment(controlFlagsSize,
                                                   CACHE_ALIGNMENT);
            if (!SharedMemForCollective->collCtlData[ele].
                collDescriptor[desc].controlFlags) {
                ulm_exit(("Unable to allocate space for controlFlags memory\n"));
            }
        }
    }
}


void lampi_init_postfork_resources(lampiState_t *s)
{

    /* if daemon - return */
    if (s->iAmDaemon) {
        return;
    }

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_resources");
    }
    //
    // Figure out which box we're on and the offset that converts a
    // global proc id to a local one.
    //

    // setup ulm's global rank to local rank mapping
    lampiState.map_global_proc_to_on_host_proc_id =
        (int *) ulm_malloc(sizeof(int) * lampiState.global_size);
    if (!lampiState.map_global_proc_to_on_host_proc_id) {
        ulm_exit(("Unable to allocate space for "
                  "lampiState.map_global_proc_to_on_host_proc_id\n"));
    }
    int offset = 0;
    for (int h = 0; h < lampiState.nhosts; ++h) {
        for (int lp = 0; lp < lampiState.map_host_to_local_size[h]; lp++) {
            lampiState.map_global_proc_to_on_host_proc_id[offset + lp] =
                lp;
        }
        offset += lampiState.map_host_to_local_size[h];
    }

    //
    // initialize send queues
    //
    IncompletePostedSends.Lock.init();
    UnackedPostedSends.Lock.init();
#if ENABLE_SHARED_MEMORY
    IncompletePostedSMPSends.Lock.init();
    UnackedPostedSMPSends.Lock.init();
#endif
    //initialize "to be sent" ack queue
    UnprocessedAcks.Lock.init();

    //
    // reliablity protocol initialization
    //
#if ENABLE_RELIABILITY
    // initialize lastCheckForRetransmits to less than zero to ensure
    // we call CheckForRetransmits at the first available time...
    lastCheckForRetransmits = -(1.0);
#endif

    //
    // Allocate memory for posted contiguous receives
    // - this will live in process private memory.
    //
    long minPagesPerList = 1;
    long nPagesPerList = 4;
    long maxPagesPerList = maxPgsIn1IRecvDescDescList;
    ssize_t pageSize = SMPPAGESIZE;
    ssize_t eleSize = (((sizeof(RecvDesc_t) - 1)
                        / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;

    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = 1;
    /* !!!! threaded lock */
    int retryForMoreResources = 1;
    int memAffinityPool = getMemPoolIndex();
    MemoryPoolPrivate_t *inputPool = NULL;

    int retVal =
        IrecvDescPool.Init(nFreeLists, nPagesPerList, poolChunkSize,
                           pageSize, eleSize,
                           minPagesPerList, maxPagesPerList,
                           maxIRecvDescRetries,
                           " IReceive descriptors ",
                           retryForMoreResources, &memAffinityPool,
                           enforceAffinity(), inputPool,
                           irecvDescDescAbortWhenNoResource);
    if (retVal) {
        ulm_exit(("FreeListsT::Init Unable to initialize IReceive "
                  "descriptor pool\n"));
    }
    // Allocate memory for double link list of pointers
    //
    minPagesPerList = 1;
    nPagesPerList = 4;
    maxPagesPerList = maxPgsIn1pointerPool;
    pageSize = SMPPAGESIZE;
    eleSize = sizeof(ptrLink_t);
    poolChunkSize = SMPPAGESIZE;
    nFreeLists = 1;
    /* !!!! threaded lock */
    retryForMoreResources = 1;
    memAffinityPool = getMemPoolIndex();
    MemoryPoolPrivate_t *inputPoolPtrL = NULL;

    retVal = pointerPool.Init
        (nFreeLists, nPagesPerList, poolChunkSize, pageSize, eleSize,
         minPagesPerList, maxPagesPerList, maxIRecvDescRetries,
         " Pointer Dbl Link List ",
         retryForMoreResources, &memAffinityPool, enforceAffinity(),
         inputPoolPtrL, pointerPoolAbortWhenNoResource);
    if (retVal) {
        ulm_exit(("FreeLists::Init Unable to initialize Dbl Link "
                  "List pool\n"));
    }
    //
    // create pool of processor group objects
    //
    // set initial pool size
    if (nIncreaseGroupArray <= 0) {
        grpPool.poolSize = 10;
    } else {
        grpPool.poolSize = nIncreaseGroupArray;
    }

    // initialize lock
    grpPool.poolLock.init();

    // set number of elements in use
    grpPool.eleInUse = 0;

    // allocate array of group pointers
    grpPool.groups = ulm_new(Group *, grpPool.poolSize);
    if (!grpPool.groups) {
        ulm_exit(("Unable to allocate space for the array of group objects\n"));
    }
    for (int grpIndex = 0; grpIndex < grpPool.poolSize; grpIndex++) {
        grpPool.groups[grpIndex] = 0;

    }                           // end grpIndex loop

    grpPool.firstFreeElement = 0;

    //
    // create pool of processor attribute objects
    //
    // set initial pool size
    attribPool.poolSize = Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT;

    // initialize lock
    attribPool.poolLock.init();

    // set number of elements in use
    attribPool.eleInUse = 0;

    // allocate array of group pointers
    attribPool.attributes =
        (attributes_t *) ulm_malloc(sizeof(attributes_t) *
                                    attribPool.poolSize);
    if (!attribPool.attributes) {
        ulm_exit(("Unable to allocate space for the array of attributes objects\n"));
    }

    // initialize locks
    for (int attr = 0; attr < attribPool.poolSize; attr++) {
        attribPool.attributes[attr].Lock.init();
    }

    attribPool.firstFreeElement = 0;

    // set number of instances in use
    nCommunicatorInstancesInUse = 0;
}


void lampi_init_allforked_resources(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_allforked_resources");
    }

    lampi_init_allforked_rms(s);
}


void lampi_init_postfork_communicators(lampiState_t *s)
{
    ulm_comm_info_t communicatorData;   /* should this be global? !!!! */

    if ( s->iAmDaemon )
        return;

    //! initialize array to hold active group objects
    communicators = ulm_new(Communicator *, communicatorsArrayLen);
    if (!communicators) {
        ulm_exit(("Unable to allocate space for activeCommunicators\n"));
    }
    for (int i = 0; i < communicatorsArrayLen; i++) {
        communicators[i] = (Communicator *) NULL;
    }

    nIncreaseCommunicatorsArray = 10;
    nIncreaseGroupArray = 10;
    nCommunicatorInstancesInUse = 0;
    activeCommunicators = ulm_new(int, maxCommunicatorInstances);

    if (!activeCommunicators) {
        ulm_exit(("Unable to allocate space for activeCommunicators\n"));
    }

    for (int i = 0; i < maxCommunicatorInstances; i++) {
        activeCommunicators[i] = -1;
    }

    communicatorsLock.init();

    //
    // sanity check - the code in the rest of this routine assumes that
    //   ULM_COMM_WORLD+1=ULM_COMM_SELF for the pupose of setting up
    //   the context ID caching
    //
    assert((ULM_COMM_WORLD + 1) == ULM_COMM_SELF);

    //
    // setup ULM_COMM_SELF
    //
    // setup group
    int groupIndex;
    int myGlobalProcID = myproc();
    int errorCode = setupNewGroupObject(1, &myGlobalProcID, &groupIndex);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: setting up ULM_COMM_SELF's group\n"));
    }
    // increment group count for the remoteGroup
    grpPool.groups[groupIndex]->incrementRefCount();

    int commSelfGroup = groupIndex;

    //
    // instantiate ULM_COMM_WORLD group
    //
    int threadUsage = ULM_THREAD_MODE;

    // initialize ulm_comm_info_t structue - only data needed to
    //   setup ULM_COMM_WORLD
    //

    // number of processes in group
    communicatorData.commSize = nprocs();

    // list of prcesses in group
    communicatorData.listOfRanksInComm =
        (int *) ulm_malloc(communicatorData.commSize * sizeof(int));
    if (!communicatorData.listOfRanksInComm) {
        ulm_exit(("Unable to allocate space for communicatorData.listOfRanksInComm\n"));
    }
    for (int proc = 0; proc < communicatorData.commSize; proc++) {
        communicatorData.listOfRanksInComm[proc] = proc;
    }

    //
    // setup COMM_WORLD's group object
    //
    errorCode = setupNewGroupObject(communicatorData.commSize,
                                    communicatorData.listOfRanksInComm,
                                    &groupIndex);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: setting up COMM_WORLD's group\n"));
    }
    // increment group count for the remoteGroup
    grpPool.groups[groupIndex]->incrementRefCount();

    // setup communicator object
    int cacheList[Communicator::MAX_COMM_CACHE_SIZE];

    // set value of greatest context id in use
    //
    s->contextIDCtl->nextID =
    ULM_COMM_SELF + 1 + myproc() * CTXIDBLOCKSIZE;
    s->contextIDCtl->outOfBounds =
    (s->contextIDCtl->nextID + CTXIDBLOCKSIZE);
    s->contextIDCtl->cycleSize = CTXIDBLOCKSIZE * nprocs();
    s->contextIDCtl->idLock.init();

    errorCode = ulm_communicator_alloc(ULM_COMM_WORLD, threadUsage,
                                    groupIndex, groupIndex, 1,
                                    Communicator::MAX_COMM_CACHE_SIZE,
                                    cacheList,
                                    Communicator::INTRA_COMMUNICATOR, 0,
                                    1, 1);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: setting up ULM_COMM_WORLD\n"));
    }
    // finish setting up ULM_COMM_SELF - ULM_COMM_SELF is setup in process private memory.
    errorCode = ulm_communicator_alloc(ULM_COMM_SELF, threadUsage,
                                    commSelfGroup, commSelfGroup, 0,
                                    Communicator::MAX_COMM_CACHE_SIZE,
                                    cacheList,
                                    Communicator::INTRA_COMMUNICATOR, 0,
                                    0, 0);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit(("Error: setting up ULM_COMM_SELF\n"));
    }
}


void lampi_init_fork(lampiState_t *s)
{
    int totalLocalProcs = s->local_size;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_fork");
    }

    s->iAmDaemon = 0;
    totalLocalProcs += (s->useDaemon) ? 1 : 0;

    if (s->useDaemon)
        atexit(wait_for_children);

    s->local_pids =
        (volatile pid_t *) SharedMemoryPools.
        getMemorySegment(sizeof(pid_t) * totalLocalProcs, CACHE_ALIGNMENT);

    if (!s->local_pids) {
        ulm_exit(("Error: allocating %ld bytes of shared memory for local_pids array\n",
                  (long) (sizeof(pid_t) * totalLocalProcs)));
    }

    mb();


    // mark SharedMemoryPools as unusable
    SharedMemoryPools.poolOkToUse_m = false;

    s->local_pids[0] = getpid();
    /* control flag set to increment */
    if (totalLocalProcs > 4) {
        s->local_rank = fork_many(totalLocalProcs,
                                  FORK_MANY_TYPE_TREE,
                                  s->local_pids);
    } else {
        s->local_rank = fork_many(totalLocalProcs,
                                  FORK_MANY_TYPE_LINEAR,
                                  s->local_pids);
    }
    mb();

    set_sa_restart();
    if (s->local_rank < 0) {
        s->error = ERROR_LAMPI_INIT_AT_FORK;
        return;
    }

    if (s->useDaemon) {
        /* adjust rank to account for daemon process */
        s->local_rank--;
        if (s->local_rank == -1) {
            s->iAmDaemon = 1;
        } else {
            s->IAmAlive[s->local_rank] = 1;
        }
    }

    /*
     * Set global to local process rank offset
     */

    s->global_to_local_offset = 0;
    for (int h = 0; h < s->nhosts; ++h) {
        if (h == s->hostid) {
            break;
        }
        s->global_to_local_offset += s->map_host_to_local_size[h];
    }
    if (s->iAmDaemon) {
        lampiState.global_rank = -1;
    } else {
        lampiState.global_rank =
            lampiState.local_rank + lampiState.global_to_local_offset;
    }

    /* wait until all local processes have started */
    lampiState.client->localBarrier();

    if (s->iAmDaemon) {
        /* repack local_pids array to include only non-daemon processes */
        for (int i = 0; i < s->local_size; i++) {
            s->local_pids[i] = s->local_pids[i + 1];
        }
    }
}


void lampi_init_prefork_connect_to_daemon(lampiState_t *s)
{
    enum { BUFSZ = 512 };
    char stderr_fifoname[BUFSZ];
    char stdout_fifoname[BUFSZ];

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_connect_to_daemon");
    }

    /*
     * Create fifos
     */

    snprintf(stderr_fifoname, sizeof(stderr_fifoname),
             "/tmp/lampid.%d/stderr.%d", getpgrp(), getpid());
    snprintf(stdout_fifoname, sizeof(stdout_fifoname),
             "/tmp/lampid.%d/stderr.%d", getpgrp(), getpid());

    if (mkfifo(stderr_fifoname, S_IRWXU) < 0) {
        s->error = ERROR_LAMPI_INIT_CONNECT_TO_DAEMON;
    }

    if (mkfifo(stdout_fifoname, S_IRWXU) < 0) {
        s->error = ERROR_LAMPI_INIT_CONNECT_TO_DAEMON;
    }

    if (s->error) {
        unlink(stderr_fifoname);
        unlink(stdout_fifoname);
    }
}


void lampi_init_prefork_connect_to_mpirun(lampiState_t *s)
{
    int auth[3], port;
    char *adminHost;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_connect_to_mpirun");
    }

    ulm_dbg(("host %d: daemon %d: connecting to mpirun...\n", myhost(),
             getpid()));

    lampi_environ_find_integer("LAMPI_ADMIN_AUTH0", &(auth[0]));
    lampi_environ_find_integer("LAMPI_ADMIN_AUTH1", &(auth[1]));
    lampi_environ_find_integer("LAMPI_ADMIN_AUTH2", &(auth[2]));
    lampi_environ_find_integer("LAMPI_ADMIN_PORT", &port);

    s->client = new adminMessage;
    lampi_environ_find_string("LAMPI_ADMIN_IP", &adminHost);
    if (!s->client->clientInitialize(auth, adminHost, port)) {
        s->error = ERROR_LAMPI_INIT_CONNECT_TO_MPIRUN;
        return;
    }
    if (!s->client->clientConnect(s->local_size, s->hostid, MIN_CONNECT_ALARMTIME)) {
        s->error = ERROR_LAMPI_INIT_CONNECT_TO_MPIRUN;
        return;
    }
}


void lampi_init_prefork_receive_setup_params(lampiState_t *s)
{
    int tag, errorCode, recvd, h, i, p;
    int pathcnt, *paths;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params");
    }

    // RUNPARAMS exchange
    // read the start of input parameters tag
    recvd = s->client->receive(-1, &tag, &errorCode);
    if ((recvd == adminMessage::OK) && (tag == adminMessage::RUNPARAMS)) {
    } else {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->receive(-1, &tag, &errorCode);
        if (recvd != adminMessage::OK) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        switch (tag) {
            /* more than one set of messages may have this tag -
             *   one set of input params are sent via a broadcast,
             *   and the other set of data is per host unique
             *   data
             */
        case adminMessage::RUNPARAMS:
            break;
        case adminMessage::ENDRUNPARAMS:
            /* done reading input params */
            done = 1;
            break;
        case adminMessage::NHOSTS:
            /* number of hosts, and number of procs per host */
            s->client->unpack(&(s->nhosts),
                              (adminMessage::packType) sizeof(int), 1);
            s->client->setNumberOfHosts(s->nhosts);
            s->map_host_to_local_size = ulm_new(int, s->nhosts);
            s->client->unpack(s->map_host_to_local_size,
                              (adminMessage::packType) sizeof(int),
                              s->nhosts);
            break;
        case adminMessage::HOSTID:
            s->client->unpack(&(s->hostid),
                              (adminMessage::packType) sizeof(int), 1);
            s->client->setHostRank(s->hostid);
            break;
        case adminMessage::DEBUGGER:
            s->client->unpack(&(s->started_under_debugger),
                              (adminMessage::packType) sizeof(int), 1);
            s->client->unpack(&(s->wait_for_debugger_in_daemon),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::THREADUSAGE:
            s->client->unpack(&(s->usethreads),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::CRC:
            s->client->unpack(&(s->usecrc),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::CHECKARGS:
            s->client->unpack(&lampiState.checkargs,
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::OUTPUT_PREFIX:
            s->client->unpack(&(s->output_prefix),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::QUIET:
            s->client->unpack(&(s->quiet),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::VERBOSE:
            s->client->unpack(&(s->verbose),
                              (adminMessage::packType) sizeof(int), 1);
            if (s->verbose) {
                s->output_prefix = 1;
            }
            break;
        case adminMessage::WARN:
            s->client->unpack(&(s->warn),
                              (adminMessage::packType) sizeof(int), 1);
            ulm_warn_enabled = s->warn;
            break;
        case adminMessage::ISATTY:
            s->client->unpack(&(s->isatty),
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case adminMessage::HEARTBEATPERIOD:
            s->client->unpack(&(s->HeartbeatPeriod),
                              (adminMessage::packType) sizeof(int), 1);
            break;

#if ENABLE_NUMA
        case adminMessage::CPULIST:
            // list of cpus to use
        {
            int c, cpulistlen, *cpulist;
            constraint_info my_c;
            s->client->unpack(&cpulistlen,
                              (adminMessage::packType) sizeof(int), 1);

            cpulist = ulm_new(int, cpulistlen);
            s->client->unpack(cpulist,
                              (adminMessage::packType) sizeof(int),
                              cpulistlen);
            my_c.reset_mask(C_MUST_USE);
            my_c.set_type(R_CPU);

            for (c = 0; c < cpulistlen; c++) {
                my_c.set_num(cpulist[c]);
                ULMreq->add_constraint(R_CPU, 1, &my_c);
            }
            ulm_delete(cpulist);
        }
        break;

        case adminMessage::NCPUSPERNODE:
            /* number of cpus per node to use */
            s->client->unpack(&nCpPNode,
                              (adminMessage::packType) sizeof(int), 1);
            if ((nCpPNode < 0) || (nCpPNode > 2)) {
            ulm_exit(("Incorrect value for nCpPNode :: %d\n",
                      nCpPNode));
        }
        break;

    case adminMessage::USERESOURCEAFFINITY:
        /* specify if resource affinity is to be used */
        s->client->unpack(&tmpInt,
                          (adminMessage::packType) sizeof(int), 1);
        if ((tmpInt < 0) || (tmpInt > 1)) {
        ulm_exit(("Incorrect value for USERESOURCEAFFINITY (%d)\n",
                  tmpInt));
    }
    if (tmpInt == 1) {
        useRsrcAffinity = true;
    } else {
        useRsrcAffinity = false;
    }
    break;
case adminMessage::DEFAULTRESOURCEAFFINITY:
    /* specify how to obtain resource affinity list */
    s->client->unpack(&tmpInt,
                      (adminMessage::packType) sizeof(int), 1);
    if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit(("Incorrect value for DEFAULTRESOURCEAFFINITY (%d)\n",
                          tmpInt));
            }
            if (tmpInt == 1) {
                useDfltAffinity = true;
            } else {
                useDfltAffinity = false;
            }
            break;
        case adminMessage::MANDATORYRESOURCEAFFINITY:
            /* specify if resource affinity usage is mandatory */
            s->client->unpack(&tmpInt,
                              (adminMessage::packType) sizeof(int), 1);
            if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit(("Incorrect value for MANDATORYRESOURCEAFFINITY (%d)\n",
                          tmpInt));

            }
            if (tmpInt == 1) {
                affinMandatory = true;
            } else {
                affinMandatory = false;
            }
            break;
#endif /* ENABLE_NUMA */

        case adminMessage::MAXCOMMUNICATORS:
            /* specify upper limit on communicators */
            s->client->unpack(&maxCommunicatorInstances,
                              (adminMessage::packType) sizeof(int), 1);
            if (maxCommunicatorInstances < 0) {
                ulm_exit(("Incorrect value for MAXCOMMUNICATORS (%d)\n",
                          maxCommunicatorInstances));
            }
            break;

        case adminMessage::NPATHTYPES:
            s->client->unpack(&pathcnt,
                              (adminMessage::packType) sizeof(int), 1);
            if (pathcnt) {
                paths = ulm_new(int, pathcnt);
                s->client->unpack(paths,
                                  (adminMessage::packType) sizeof(int),
                                  pathcnt);
                for (int i = 0; i < pathcnt; i++) {
                    switch (paths[i]) {
                    case PATH_UDP:
                        s->udp = 1;
                        break;
                    case PATH_TCP:
                        s->tcp = 1;
                        break;
                    case PATH_GM:
                        s->gm = 1;
                        break;
                    case PATH_QUADRICS:
                        s->quadrics = 1;
                        break;
                    case PATH_IB:
                        s->ib = 1;
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case adminMessage::IRECVOUTOFRESRCABORT:
            /* what to do when out of receive descriptors */
            irecvDescDescAbortWhenNoResource = false;
            break;

        case adminMessage::IRECVRESOURCERETRY:
            /* how many times to retry to get more receive
             *   descriptors when they are temporaraly unavailable */
            s->client->unpack(&maxIRecvDescRetries,
                              (adminMessage::packType) sizeof(long), 1);
            break;
        case adminMessage::IRCEVMINPAGESPERCTX:
            /* minimum number of pages allocated to each
             *   list of receive descirptors */
            s->client->unpack(&minPgsIn1IRecvDescDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;

        case adminMessage::IRECVMAXPAGESPERCTX:
            /* maximum number of pages allocated to each
             *   list of receive descirptors */
            s->client->unpack(&maxPgsIn1IRecvDescDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;

        default:
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
    }

    /* decide if a daemon will be used */
#if ENABLE_RMS
    s->useDaemon = 0;
#else
    s->useDaemon = 1;
#endif

    /* set local size */
    s->local_size = s->map_host_to_local_size[s->hostid];

    /* compute global_size */
    s->global_size = 0;
    for (h = 0; h < lampiState.nhosts; ++h) {
        s->global_size += s->map_host_to_local_size[h];
    }
    s->client->setTotalNumberOfProcesses(s->global_size);

    if (s->useDaemon) {
        s->IAmAlive = (int *)
            SharedMemoryPools.getMemorySegment((s->local_size + 1) *
                                               sizeof(int), PAGESIZE);
#ifdef HAVE_SYS_RESOURCE_H
        if (s->useDaemon) {
            s->rusage = (struct rusage *)
                SharedMemoryPools.getMemorySegment(s->local_size *
                                                   sizeof(struct rusage),
                                                   PAGESIZE);
            memset(s->rusage, 0, s->local_size * sizeof(struct rusage));
        }
#endif

        s->AbnormalExit = (abnormalExitInfo *)
            SharedMemoryPools.getMemorySegment(sizeof(abnormalExitInfo), PAGESIZE);
        memset(s->AbnormalExit, 0, sizeof(abnormalExitInfo));
        ATOMIC_LOCK_INIT(s->AbnormalExit->lock);
    }
    // allocate cpus - need to have this setup for memory locality
    //   initialization.
    errorCode = getCPUSet();
    if (errorCode != ULM_SUCCESS) {
        ulm_err(("getCPUSet failed\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }


    /*
     * Allocate and initialize AllHostsDone flag
     */
    lampiState.sync.AllHostsDone =
        (int *) SharedMemoryPools.getMemorySegment(sizeof(int),
                                                   CACHE_ALIGNMENT);
    assert(lampiState.sync.AllHostsDone != (volatile int *) 0);
    *lampiState.sync.AllHostsDone = 0;

    /*
     * Build an array that maps from global process
     * number to the box number.
     */
    s->map_global_rank_to_host = ulm_new(int, nprocs());
    p = 0;
    for (h = 0; h < s->nhosts; ++h)
        for (i = 0; i < s->map_host_to_local_size[h]; ++i)
            s->map_global_rank_to_host[p++] = h;

    /*
     * receive path specific information
     */
    lampi_init_prefork_receive_setup_params_shared_memory(s);
    lampi_init_prefork_receive_setup_params_quadrics(s);
    lampi_init_prefork_receive_setup_params_gm(s);
    lampi_init_prefork_receive_setup_params_ib(s);
    lampi_init_prefork_receive_setup_params_tcp(s);

}


void lampi_init_prefork_paths(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_paths");
    }

    lampi_init_prefork_shared_memory(s);        /* must be first */

    /* 
     * as long as mpirun participates in the "wire up", the order
     * these paths are initialized needs to be mirrored in mpirun
     */
    lampi_init_prefork_udp(s);
    lampi_init_prefork_tcp(s);
    lampi_init_prefork_quadrics(s);
    lampi_init_prefork_gm(s);
}


void lampi_init_postfork_paths(lampiState_t *s)
{
    int pathCount;
    pathType ptype;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_paths");
    }

    /*
     * create global pathContainer in local process private memory
     */

    lampiState.pathContainer = new pathContainer_t;
    if (!lampiState.pathContainer) {
        s->error = ERROR_LAMPI_INIT_POSTFORK_PATHS;
        return;
    }

    /*
     * path specific initialization
     */

    lampi_init_postfork_shared_memory(s);       /* must be first */
    lampi_init_postfork_udp(s);
    lampi_init_postfork_tcp(s);
    lampi_init_postfork_quadrics(s);
    lampi_init_postfork_gm(s);
    lampi_init_postfork_ib(s);

    /*
     * setup global array of available paths
     */
    pathList = (availablePaths_t *) ulm_malloc(sizeof(availablePaths_t) *
                                               s->global_size);
    if (!pathList) {
        s->error = ERROR_LAMPI_INIT_POSTFORK_PATHS;
        return;
    }

    pathCount = pathContainer()->allPaths(pathArray, MAX_PATHS);

    for (int proc = 0; proc < s->global_size; proc++) {
        pathList[proc].useSharedMemory_m = -1;
        pathList[proc].useTCP_m = -1;
        pathList[proc].useUDP_m = -1;
        pathList[proc].useQuadrics_m = -1;
        pathList[proc].useGM_m = -1;
        pathList[proc].useIB_m = -1;

        for (int i = 0; i < pathCount; i++) {
            if (!(lampiState.pathContainer->canUsePath(proc, i)))
                continue;
            if (pathArray[i]->getInfo(PATH_TYPE, 0, &ptype,
                                      sizeof(pathType), &(s->error))) {
                if ((ptype == SHAREDMEM)
                    && (pathList[proc].useSharedMemory_m < 0)) {
                    pathList[proc].useSharedMemory_m = i;
                } else if ((ptype == TCPPATH)
                           && (pathList[proc].useTCP_m < 0)) {
                    pathList[proc].useTCP_m = i;
                } else if ((ptype == UDPPATH)
                           && (pathList[proc].useUDP_m < 0)) {
                    pathList[proc].useUDP_m = i;
                } else if ((ptype == QUADRICSPATH)
                           && (pathList[proc].useQuadrics_m < 0)) {
                    pathList[proc].useQuadrics_m = i;
                } else if ((ptype == GMPATH)
                           && (pathList[proc].useGM_m < 0)) {
                    pathList[proc].useGM_m = i;
                } else if ((ptype == IBPATH)
                           && (pathList[proc].useIB_m < 0)) {
                    pathList[proc].useIB_m = i;
                }
            }
        }
    }                           /* end proc loop */
}


void lampi_init_wait_for_start_message(lampiState_t *s)
{
    int tag, errorCode, goahead;
    bool r;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_wait_for_start_message");
    }

    /* barrier */
    s->client->localBarrier();

    /* daemon process, if it exists, or process 0 in the absence of a daemon
     *   participates in the interhost barrier
     */
    if ((s->useDaemon && s->iAmDaemon) ||
        (!s->useDaemon && (local_myproc() == 0))) {
        r = s->client->reset(adminMessage::SEND);
        r = r && s->client->send(-1, adminMessage::BARRIER, &errorCode);
        r = r
            && (s->client->receive(-1, &tag, &errorCode) ==
                adminMessage::OK);
        r = r && (tag == adminMessage::BARRIER)
            && s->client->unpack(&goahead,
                                 (adminMessage::packType) sizeof(int), 1);
        if (!r || !goahead) {
            s->error = ERROR_LAMPI_INIT_WAIT_FOR_START_MESSAGE;
        }
    }

    s->client->localBarrier();
}


void lampi_init_postfork_pids(lampiState_t *s)
{
    if (s->error) {
        return;
    }

    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_pids");
    }

    if ((s->useDaemon && s->iAmDaemon) ||
        (!s->useDaemon && (s->local_rank == 0))) {

        char version[ULM_MAX_VERSION_STRING];
        int errorCode;
        int tag;
        int goahead;

        memset(version, '\0', sizeof(version));
        strncpy(version, PACKAGE_VERSION, sizeof(version) - 1);

        s->client->reset(adminMessage::SEND);
        s->client->pack((void *) s->local_pids,
                        (adminMessage::packType) sizeof(pid_t),
                        s->local_size);
        s->client->pack((void *) version,
                        (adminMessage::packType) sizeof(char),
                        sizeof(version));
        s->client->send(-1, adminMessage::CLIENTPIDS, &errorCode);

        /* wait for goahead from mpirun */

        if (!(s->client->receive(-1, &tag, &errorCode) == adminMessage::OK
              && tag == adminMessage::BARRIER
              && s->client->unpack(&goahead,
                                   (adminMessage::packType) sizeof(int), 1)
              && goahead)) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_PIDS;
        }
    }

    s->client->localBarrier();
}


void lampi_init_prefork_check_stdio(lampiState_t *s)
{
    int nullfd;
    struct stat dummy_stat;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_stdio");
    }

    /* do nothing if stdio is not being managed */
    if (!s->interceptSTDio) {
        return;
    }

    /*
     * try to make sure stdin/stdout/stderr exist....
     */
    if (fstat(STDIN_FILENO, &dummy_stat) < 0) {
        nullfd = open("/dev/null", O_RDONLY);
        if ((nullfd >= 0) && (STDIN_FILENO != nullfd)) {
            dup2(nullfd, STDIN_FILENO);
            close(nullfd);
        }
    }
    if (fstat(STDOUT_FILENO, &dummy_stat) < 0) {
        nullfd = open("/dev/null", O_WRONLY);
        if ((nullfd >= 0) && (STDOUT_FILENO != nullfd)) {
            dup2(nullfd, STDOUT_FILENO);
            close(nullfd);
        }
    }
    if (fstat(STDERR_FILENO, &dummy_stat) < 0) {
        nullfd = open("/dev/null", O_WRONLY);
        if ((nullfd >= 0) && (STDERR_FILENO != nullfd)) {
            dup2(nullfd, STDERR_FILENO);
            close(nullfd);
        }
    }
}

void lampi_init_prefork_stdio(lampiState_t *s)
{
    int fd[2];
    int enable_pty_stdio;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_stdio");
    }

    /* only need to use pty's if mpirun is a tty */
    if (0) {
    if (s->isatty) {
        enable_pty_stdio = ENABLE_PTY_STDIO;
    } else {
        enable_pty_stdio = 0;
    }
    } else {
        enable_pty_stdio = ENABLE_PTY_STDIO;
    }

    /* allocate pipe for determining when all processes have exited... */
    if(pipe(s->commonAlivePipe) < 0) {
        ulm_err(("Error: pipe(): errno = %d\n", errno));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }

    /* do nothing else if stdio is not being managed */
    if (!s->interceptSTDio) {
        return;
    }

    /*
     * allocate space for pipes
     */
    s->STDOUTfdsFromChildren = ulm_new(int, s->local_size + 1);
    s->STDERRfdsFromChildren = ulm_new(int, s->local_size + 1);
    if (!s->STDOUTfdsFromChildren ||
        !s->STDERRfdsFromChildren) {
        ulm_err(("Error: Out of memory\n"));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }

    /*
     * setup stdin/stdout/stderr redirection
     */
    stdout_parent = ulm_new(int, s->local_size);
    stdout_child = ulm_new(int, s->local_size);
    stderr_parent = ulm_new(int, s->local_size);
    stderr_child = ulm_new(int, s->local_size);
    if (!stdout_parent || !stdout_child || !stderr_parent || !stderr_child) {
        ulm_err(("Error: Out of memory\n"));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }

    if (enable_pty_stdio && s->isatty) {
        /* use a pty for stdin to avoid application buffering */
        if (openpty(&fd[1], &fd[0], NULL, NULL, NULL) < 0) {
            ulm_warn(("Warning: openpty failed: using pipe for stdio\n"));
            enable_pty_stdio = 0;
        }
    }
    if (!(enable_pty_stdio && s->isatty)) {
        if(pipe(fd) < 0) {
            ulm_err(("Error: pipe(): errno = %d\n", errno));
            s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
            return;
        }
    }
    stdin_parent = fd[1];
    stdin_child = fd[0];

    for (int i = 0; i < s->local_size; i++) {
        if (enable_pty_stdio) {
            /* use a pty for stdout to avoid application buffering */
            if (openpty(&fd[0], &fd[1], NULL, NULL, NULL) < 0) {
                ulm_warn(("Warning: openpty failed: using pipe for stdio\n"));
                enable_pty_stdio = 0;
            }
        }
        if (!enable_pty_stdio) {
            if (pipe(fd) < 0) {
                ulm_err(("Error: pipe(): errno = %d\n", errno));
                s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
                return;
            }
        }
        stdout_parent[i] = fd[0];
        stdout_child[i] = fd[1];
    }

    for (int i = 0; i < s->local_size; i++) {
        if (pipe(fd) < 0) {
            ulm_err(("Error: pipe(): errno = %d\n", errno));
            s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
            return;
        }
        stderr_parent[i] = fd[0];
        stderr_child[i] = fd[1];
    }

    /*
     * create pipes for stdout/stderr from the daemon to mpirun
     */

    if (pipe(fd) < 0) {
        ulm_err(("Error: pipe(): errno = %d\n", errno));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }
    fflush(stdout);
    s->STDOUTfdsFromChildren[s->local_size] = fd[0];
    if (dup2(fd[1], STDOUT_FILENO) < 0) {
        ulm_err(("Error: dup2(): errno = %d\n", errno));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }
    close(fd[1]);

    if (pipe(fd) < 0) {
        ulm_err(("Error: pipe(): errno = %d\n", errno));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }
    fflush(stderr);
    s->STDERRfdsFromChildren[s->local_size] = fd[0];
    if (dup2(fd[1], STDERR_FILENO) < 0) {
        ulm_err(("Error: dup2(): errno = %d\n", errno));
        s->error = ERROR_LAMPI_INIT_PREFORK_STDIO;
        return;
    }
    close(fd[1]);

    /*
     * setup array holding prefix data for stdio data coming from
     * "worker" processes
     */

    s->IOPreFix = ulm_new(PrefixName_t, s->local_size + 1);
    s->LenIOPreFix = ulm_new(int, s->local_size + 1);
    s->NewLineLast = ulm_new(int, s->local_size + 1);
    for (int i = 0; i <= s->local_size; i++) {
        /* null prefix */
        s->IOPreFix[i][0] = '\0';
        s->LenIOPreFix[i] = 0;
        s->NewLineLast[i] = 1;
    }

    s->StderrBytesWritten = 0;
    s->StdoutBytesWritten = 0;
}


void lampi_init_postfork_stdio(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_stdio");
    }

    /*
     * daemon holds read end of pipe...all other process hold write
     * end together
     */
    if (s->iAmDaemon) {
        close(s->commonAlivePipe[1]);
        s->commonAlivePipe[1] = -1;
    }
    else {
        close(s->commonAlivePipe[0]);
        s->commonAlivePipe[0] = -1;
    }

    /* explicitly turn off buffering of my stdio streams */
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    /* do nothing else if stdio is not being managed */
    if (!s->interceptSTDio) {
        return;
    }

    if (s->iAmDaemon) {

        if (s->output_prefix) {
            /* activate prefix */

            struct utsname utsname;

            uname(&utsname);
 
            for (int i = 0; i < s->local_size; i++) {
                sprintf(s->IOPreFix[i], "%d[%d.%d.%s] ",
                        i + s->global_to_local_offset,
                        s->hostid, i,
                        utsname.nodename);
                s->LenIOPreFix[i] = (int) strlen(s->IOPreFix[i]);
            }
            sprintf(s->IOPreFix[s->local_size], "daemon[%d] ", s->hostid);
            s->LenIOPreFix[s->local_size] = (int) strlen(s->IOPreFix[s->local_size]);
        }

        /* setup stdin */
        if(s->hostid != 0) {
            s->STDINfdToChild = -1;
            close(stdin_parent);
            close(stdin_child);
        } else {
            s->STDINfdToChild = stdin_parent;
            close(stdin_child);
            if (isatty(stdin_parent)) {
                tty_raw(stdin_parent);
            }

            /* setup write end of pipe to be non-blocking */
            int flags;
            if((flags = fcntl(s->STDINfdToChild, F_GETFL, 0)) < 0) {
                ulm_err(("lampi_init_postfork_stdio: fcntl(F_GETFL) failed with errno=%d\n", errno));
            } else {
                flags |= O_NONBLOCK;
                if(fcntl(s->STDINfdToChild, F_SETFL, flags) < 0)
                    ulm_err(("lam_init_postfork_stdio: fcntl(F_SETFL) failed with errno=%d\n", errno));
            }
        }

        /* close all write stderr/stdout pipe fd's ) */
        for (int i = 0; i < s->local_size; i++) {
            close(stdout_child[i]);
            close(stderr_child[i]);
            s->STDOUTfdsFromChildren[i] = stdout_parent[i];
            s->STDERRfdsFromChildren[i] = stderr_parent[i];
        }

    } else {

        /* setup stdin */
        if(s->global_rank == 0) {
            if (dup2(stdin_child, STDIN_FILENO) < 0) {
                ulm_err(("Error: dup2(): errno = %d\n", errno));
                s->error = ERROR_LAMPI_INIT_POSTFORK_STDIO;
                return;
            }
        } else {
            close(STDIN_FILENO);
        }
        close(stdin_parent);
        close(stdin_child);

        /* setup "application process" handling of stdout/stderr */
        if (dup2(stdout_child[s->local_rank], STDOUT_FILENO) < 0) {
            ulm_err(("Error: dup2(): errno = %d\n", errno));
            s->error = ERROR_LAMPI_INIT_POSTFORK_STDIO;
            return;
        }

        if (isatty(STDOUT_FILENO)) {
            tty_raw(STDOUT_FILENO);
        }

        if (dup2(stderr_child[s->local_rank], STDERR_FILENO) < 0) {
            ulm_err(("Error: dup2(): errno = %d\n", errno));
            s->error = ERROR_LAMPI_INIT_POSTFORK_STDIO;
            return;
        }

        for (int i = 0; i < s->local_size; i++) {
            /* close all extra descriptors */
            close(stdout_child[i]);
            close(stderr_child[i]);
            close(stdout_parent[i]);
            close(stderr_parent[i]);
        }
        /* don't forget the daemon's std. out/err pipes to itself */
        close(s->STDERRfdsFromChildren[s->local_size]);
        close(s->STDOUTfdsFromChildren[s->local_size]);
    }

    ulm_delete(stdout_parent);
    ulm_delete(stdout_child);
    ulm_delete(stderr_parent);
    ulm_delete(stderr_child);
}


void lampi_init_prefork_initialize_state_information(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_initialize_state_information");
    }

    memset(s, 0, sizeof(lampiState_t));
    s->error = LAMPI_INIT_SUCCESS;
    s->verbose = 0;
    s->warn = 0;
    s->started_under_debugger = 0;
    s->wait_for_debugger_in_daemon = 0;
    s->interceptSTDio = 1;

    /* network path types */
    s->quadrics = false;
    s->gm = 0;
    s->udp = 0;
    s->tcp = 0;
    s->ib = 0;

    /* MPI buffered send data */
    s->bsendData = (bsendData_t *) ulm_malloc((sizeof(bsendData_t)));
    if (!s->bsendData) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    /* Context ID allocation control */
    s->contextIDCtl = (contextIDCtl_t *) ulm_malloc((sizeof(contextIDCtl_t)));
    if (!s->contextIDCtl) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }
    memset(s->contextIDCtl, 0, sizeof(contextIDCtl_t));
}


void lampi_init_prefork_ip_addresses(lampiState_t *s)
{
    int errorCode;
    int recvd;
    int tag;

    if (s->error) {
        return;
    }

    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_ip_addresses");
    }

    if ((s->nhosts == 1) || (!s->tcp && !s->udp)) {
        return;
    }

    recvd = s->client->receive(-1, &tag, &errorCode);
    if (recvd != adminMessage::OK || tag != adminMessage::IFNAMES) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    /* retrieve number of interfaces */
    if (s->client->unpack(&s->if_count,
                          (adminMessage::packType) sizeof(s->if_count),
                          1) != true) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    if (s->if_count) {

        /* retreive list of interface names */
        size_t size = s->if_count * sizeof(InterfaceName_t);
        s->if_names = (InterfaceName_t *) ulm_malloc(size);
        if (s->if_names == 0) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        if (s->client->unpack(s->if_names, adminMessage::BYTE, size) != true) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        size = s->if_count * sizeof(struct sockaddr_in);
        s->if_addrs = (struct sockaddr_in *) ulm_malloc(size);
        if (s->if_addrs == 0) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        memset(s->if_addrs, 0, size);

        /* resolve IP address assigned to each IF */
        for (int i = 0; i < s->if_count; i++) {
            HostName_t hostName;
            if (ulm_ifnametoaddr
                (s->if_names[i], hostName,
                 sizeof(hostName)) != ULM_SUCCESS) {
                s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
                return;
            }
            in_addr_t inaddr = inet_addr(hostName);
            if (inaddr == INADDR_ANY) {
                struct hostent *host = gethostbyname(hostName);
                if (host == 0) {
                    s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
                    return;
                }
                memcpy(&inaddr, host->h_addr, sizeof(inaddr));
            }
            s->if_addrs[i].sin_family = AF_INET;
            s->if_addrs[i].sin_addr.s_addr = inaddr;
        }

    } else {

        /* no interface list specified so use local hostname */
        s->if_count = 1;
        s->if_names =
            (InterfaceName_t *) ulm_malloc(sizeof(InterfaceName_t));
        s->if_addrs =
            (struct sockaddr_in *) ulm_malloc(sizeof(struct sockaddr_in));
        if (s->if_names == 0 || s->if_addrs == 0) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        memset(s->if_names, 0, sizeof(InterfaceName_t));
        memset(s->if_addrs, 0, sizeof(struct sockaddr_in));

        HostName_t hostName;
        if (gethostname(hostName, sizeof(hostName)) < 0) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        if (ulm_ifaddrtoname
            (hostName, s->if_names[0],
             sizeof(InterfaceName_t)) != ULM_SUCCESS) {
            //s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            //return;
        }

        struct hostent *host = gethostbyname(hostName);
        if (host == 0) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        s->if_addrs[0].sin_family = AF_INET;
        memcpy(&s->if_addrs[0].sin_addr, host->h_addr, sizeof(in_addr_t));
    }
}


void lampi_init_postfork_ip_addresses(lampiState_t* s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_ip_addresses");
    }

    if ((s->nhosts == 1) || (!s->tcp && !s->udp)) {
        return;
    }

    /* redistribute list of all addresses */
    size_t count = (s->global_size * s->if_count);
    s->h_addrs =
        (struct sockaddr_in *) ulm_malloc(count *
                                          sizeof(struct sockaddr_in));
    if (s->h_addrs == 0) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    int rc = s->client->allgather(s->if_addrs, s->h_addrs,
                                  (s->if_count * sizeof(struct sockaddr_in)));
    if (rc != ULM_SUCCESS) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }
}
