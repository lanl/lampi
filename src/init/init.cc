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

/*
 * Initialization
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "queue/Communicator.h"
#include "queue/contextID.h"
#include "queue/globals.h"
#include "client/adminMessage.h"
#include "client/TV.h"
#include "client/ULMClient.h"
#include "util/Lock.h"
#include "util/MemFunctions.h"
#include "util/Utility.h"
#include "util/dclock.h"
#include "ctnetwork/CTNetwork.h"
#include "init/environ.h"
#include "init/fork_many.h"
#include "init/init.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/system.h"
#include "mem/FreeLists.h"
#include "mpi/constants.h"
#include "path/common/BaseDesc.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/pathContainer.h"
#include "os/MemoryPolicy.h"
#include "ulm/ulm.h"

#ifdef SHARED_MEMORY
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPSendDesc.h"
#endif /* SHARED_MEMORY */

#ifdef __mips
bool useRsrcAffinity = true;
bool useDfltAffinity = true;
bool affinMandatory = true;
#include "os/IRIX/acquire.h"
int nCpPNode = 2;
#endif


/*
 * File scope variables
 */

static int initialized = 0;

static int dupSTDERRfd;
static int dupSTDOUTfd;
static int *StderrPipes;
static int *StdoutPipes;



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

    /* initialize lampiState */
    lampi_init_prefork_initialize_state_information(&lampiState);

    /*
     * Initialize process
     */
    lampi_init_prefork_process_resources(&lampiState);

    /* read library environment variables */
    lampi_init_prefork_environment(&lampiState);

    /* initialization of global variables */
    lampi_init_prefork_globals(&lampiState);

    /*
     * startup specific information that needs to be set before
     * connecting back to mpirun
     */
    lampi_init_prefork_resource_management(&lampiState);

    /*
     * connect back to mpirun
     */
    ulm_dbg( ("host %s: daemon %d: connecting to mpirun...\n", _ulm_host(), getpid()) );
    lampi_init_prefork_connect_to_mpirun(&lampiState);

    /*
     * receive initial input parameters
     */
    ulm_dbg( ("host %s: node %u: recving setup params...\n", _ulm_host(), lampiState.client->nodeLabel()) );
    lampi_init_prefork_receive_setup_params(&lampiState);

    if (lampiState.hostid == 0) {
        ulm_notice(("*** LA-MPI: Copyright 2001-2003, "
                    "ACL, Los Alamos National Laboratory ***\n"));
    }

    lampi_init_prefork_debugger(&lampiState);

    lampi_init_prefork_resources(&lampiState);

    /*
     * path initialization that needs to happen before the child
     * processes are created
     */
    ulm_dbg( ("host %s: node %u: calling lampi_init_paths_prefork...\n", _ulm_host(), lampiState.client->nodeLabel()) );
    lampi_init_prefork_paths(&lampiState);

    /* if library is to handle stdio, prefork data is set up */
    lampi_init_prefork_stdio(&lampiState);

    /* fork the rest of the la-mpi processes */
    lampi_init_fork(&lampiState);
    /*
     * all la-mpi procs have been created at this stage
     */

    lampi_init_postfork_debugger(&lampiState);

    /* if library is to handle stdio, postfork data is set up */
    lampi_init_postfork_stdio(&lampiState);


    lampi_init_postfork_resource_management(&lampiState);
    lampi_init_postfork_globals(&lampiState);
    lampi_init_postfork_resources(&lampiState);
    //lampi_init_debug(&lampiState);

    /* post fork path setup */
    lampi_init_postfork_paths(&lampiState);

    /* barrier until all procs - local and remote - have started up */
    lampi_init_wait_for_start_message(&lampiState);

    lampi_init_check_for_error(&lampiState);

    /* daemon process goes into loop */
    if ( lampiState.iAmDaemon )
    {
        lampi_daemon_loop(&lampiState);
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
                fprintf(stderr, "lampi_init: Error: %s\n", lookup[i].string);
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
        }
        fprintf(stderr, "lampi_init: Error: Unknown error\n");
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
    if (lampi_environ_find_integer("LAMPI_VERBOSE", &(s->verbose))) {
        lampi_init_print("lampi_init");
        lampi_init_print("lampi_init_prefork_environment");
    }
    if (s->verbose) {
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
    s->client->setLocalProcessRank(s->local_rank);
}


void lampi_init_prefork_resource_management(lampiState_t *s)
{
    int value;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_resource_management");
    }
    /* initialize data */
    s->hostid = adminMessage::UNKNOWN_HOST_ID;
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
#ifdef SHARED_MEMORY
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
        ulm_exit((-1,
                  "Unable to allocate space for _ulm_CommunicatorRecvFrags\n"));
    }
    // allocate individual lists and initialize locks
    for (int comm_rt = 0; comm_rt < local_nprocs(); comm_rt++) {
        // allocate shared memory
        _ulm_CommunicatorRecvFrags[comm_rt] = (SharedMemDblLinkList *)
            SharedMemoryPools.getMemorySegment(sizeof(DoubleLinkList),
                                               CACHE_ALIGNMENT);
        if (!(_ulm_CommunicatorRecvFrags[comm_rt])) {
            ulm_exit((-1, "Unable to allocate space for "
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
        ulm_exit((-1, "Unable to instantiate communicators\n"
                  "Upper limit :: %d\n", maxCommunicatorInstances));
    }
    //
    // initialize pool for SW SMP barriers
    //
    int retVal = allocSWSMPBarrierPools();
    if (retVal != ULM_SUCCESS) {
        ulm_exit((-1,
                  "Error: allocating resources for SW SMP barrier pool.\n"));
    }

#ifdef RELIABILITY_ON
    // allocate info structure for reliable retransmission, etc.
    reliabilityInfo = (ReliabilityInfo *) SharedMemoryPools.
        getMemorySegment(sizeof(ReliabilityInfo), CACHE_ALIGNMENT);
    if (!reliabilityInfo) {
        ulm_exit((-1,
                  "Error: allocating resources for reliability information.\n"));
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
        ulm_exit((-1,
                  "Unable to allocate space for SharedMemForCollective *\n"));
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
                ulm_exit((-1,
                          "Unable to allocate space for collDescriptor memory\n"));
            }
            SharedMemForCollective->collCtlData[ele].collDescriptor[desc].
                controlFlags = (flagElement_t *)
                SharedMemoryPools.getMemorySegment(controlFlagsSize,
                                                   CACHE_ALIGNMENT);
            if (!SharedMemForCollective->collCtlData[ele].
                collDescriptor[desc].controlFlags) {
                ulm_exit((-1,
                          "Unable to allocate space for controlFlags memory\n"));
            }
        }
    }
}


void lampi_init_postfork_resources(lampiState_t *s)
{
    ulm_comm_info_t communicatorData;   /* should this be global? !!!! */

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
        ulm_exit((-1, "Unable to allocate space for "
                  "lampiState.map_global_proc_to_on_host_proc_id \n"));
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
#ifdef SHARED_MEMORY
    IncompletePostedSMPSends.Lock.init();
    UnackedPostedSMPSends.Lock.init();
#endif
    //initialize "to be sent" ack queue
    UnprocessedAcks.Lock.init();

    //
    // reliablity protocol initialization
    //
#ifdef RELIABILITY_ON
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
#ifdef SHARED_MEMORY
    ssize_t eleSize = (((sizeof(SMPSendDesc_t) + sizeof(RecvDesc_t) - 1)
                        / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
#else
    ssize_t eleSize = (((sizeof(RecvDesc_t) - 1)
                        / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
#endif

    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = 1;
    /* !!!! threaded lock */
    int retryForMoreResources = 1;
    int memAffinityPool = getMemPoolIndex();
    bool useInputPool = false;
    MemoryPool < MMAP_PRIVATE_PROT, MMAP_PRIVATE_FLAGS,
        MMAP_SHARED_FLAGS > *inputPool = NULL;

    int retVal =
        IrecvDescPool.Init(nFreeLists, nPagesPerList, poolChunkSize,
                           pageSize, eleSize,
                           minPagesPerList, maxPagesPerList,
                           maxIRecvDescRetries,
                           " IReceive descriptors ",
                           retryForMoreResources, &memAffinityPool,
                           enforceMemoryPolicy(), inputPool,
                           irecvDescDescAbortWhenNoResource);
    if (retVal) {
        ulm_exit((-1, "FreeListsT::Init Unable to initialize IReceive "
                  "descriptor pool\n"));
    }
    // Allocate memory for RequestDesc_t's - this will live in process
    //  private memory.
    //
    minPagesPerList = 4;
    nPagesPerList = 4;
    maxPagesPerList = -1;
    pageSize = SMPPAGESIZE;
    eleSize = sizeof(RequestDesc_t);
    poolChunkSize = SMPPAGESIZE;
    nFreeLists = 2;
    /* !!! threaded lock */
    retryForMoreResources = 1;
    memAffinityPool = getMemPoolIndex();
    useInputPool = false;
    MemoryPool < MMAP_PRIVATE_PROT, MMAP_PRIVATE_FLAGS,
        MMAP_SHARED_FLAGS > *inputPoolULMR = NULL;

    retVal = ulmRequestDescPool.Init
        (nFreeLists, nPagesPerList, poolChunkSize, pageSize, eleSize,
         minPagesPerList, maxPagesPerList, maxIRecvDescRetries,
         " RequestDesc_t pool ",
         retryForMoreResources, &memAffinityPool, enforceMemoryPolicy(),
         inputPoolULMR, irecvDescDescAbortWhenNoResource);
    if (retVal) {
        ulm_exit((-1, "FreeLists::Init Unable to initialize RequestDesc_t "
                  "pool \n"));
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
    useInputPool = false;
    MemoryPool < MMAP_PRIVATE_PROT, MMAP_PRIVATE_FLAGS,
        MMAP_SHARED_FLAGS > *inputPoolPtrL = NULL;

    retVal = pointerPool.Init
        (nFreeLists, nPagesPerList, poolChunkSize, pageSize, eleSize,
         minPagesPerList, maxPagesPerList, maxIRecvDescRetries,
         " Pointer Dbl Link List ",
         retryForMoreResources, &memAffinityPool, enforceMemoryPolicy(),
         inputPoolPtrL, pointerPoolAbortWhenNoResource);
    if (retVal) {
        ulm_exit((-1, "FreeLists::Init Unable to initialize Dbl Link "
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
        ulm_exit((-1,
                  "Unable to allocate space for the array of group objects\n"));
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
        ulm_exit((-1,
                  "Unable to allocate space for the array of attributes objects\n"));
    }
    // initizliae locks
    for (int attr = 0; attr < attribPool.poolSize; attr++) {
        attribPool.attributes[attr].Lock.init();
    }

    attribPool.firstFreeElement = 0;

    // set number of instances in use
    nCommunicatorInstancesInUse = 0;

    //! initialize array to hold active group objects
    communicators = ulm_new(Communicator *, communicatorsArrayLen);
    if (!communicators) {
        ulm_exit((-1,
                  "Unable to allocate space for activeCommunicators\n"));
    }
    for (int i = 0; i < communicatorsArrayLen; i++) {
        communicators[i] = (Communicator *) NULL;
    }

    nIncreaseCommunicatorsArray = 10;
    nIncreaseGroupArray = 10;
    nCommunicatorInstancesInUse = 0;
    activeCommunicators = ulm_new(int, maxCommunicatorInstances);

    if (!activeCommunicators) {
        ulm_exit((-1,
                  "Unable to allocate space for activeCommunicators\n"));
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
    /*
       if( (ULM_COMM_WORLD+1) != ULM_COMM_SELF ) {
       ulm_exit((-1," Internal Error ::\n"
       " ULM_COMM_WORLD %d ULM_COMM_SELF %d\n"));
       }
     */

    //
    // setup ULM_COMM_SELF
    //
    // setup group
    int groupIndex;
    int myGlobalProcID = myproc();
    int errorCode = setupNewGroupObject(1, &myGlobalProcID, &groupIndex);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit((-1, "Error: setting up ULM_COMM_SELF's group\n"));
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
        ulm_exit((-1,
                  "Unable to allocate space for communicatorData.listOfRanksInComm\n"));
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
        ulm_exit((-1, "Error: setting up COMM_WORLD's group\n"));
    }
    // increment group count for the remoteGroup
    grpPool.groups[groupIndex]->incrementRefCount();

    // setup communicator object
    int cacheList[Communicator::MAX_COMM_CACHE_SIZE];
    // fill in the cache list
    //for (int cm = 1; cm <= Communicator::MAX_COMM_CACHE_SIZE; cm++) {
    //    cacheList[cm - 1] = ULM_COMM_SELF + cm;
    //}
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
        ulm_exit((-1, "Error: setting up ULM_COMM_WORLD\n"));
    }
    // finish setting up ULM_COMM_SELF - ULM_COMM_SELF is setup in process private memory.
    errorCode = ulm_communicator_alloc(ULM_COMM_SELF, threadUsage,
                                       commSelfGroup, commSelfGroup, 0,
                                       Communicator::MAX_COMM_CACHE_SIZE,
                                       cacheList,
                                       Communicator::INTRA_COMMUNICATOR, 0,
                                       0, 0);
    if (errorCode != ULM_SUCCESS) {
        ulm_exit((-1, "Error: setting up ULM_COMM_SELF\n"));
    }
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

    s->local_pids =
        (volatile pid_t *) SharedMemoryPools.
        getMemorySegment(sizeof(pid_t) * totalLocalProcs, CACHE_ALIGNMENT);

    if (!s->local_pids) {
        ulm_exit((-1,
                  "Error: allocating %ld bytes of shared memory for local_pids array\n",
                  (long) (sizeof(pid_t) * totalLocalProcs)));
    }

    mb();


    // mark SharedMemoryPools as unusable
    SharedMemoryPools.poolOkToUse_m = false;

    s->local_pids[0] = getpid();
    /* control flag set to increment */
    s->local_rank = fork_many(totalLocalProcs, FORK_MANY_TYPE_TREE,
                              s->local_pids);
    mb();

    if (s->local_rank < 0) {
        s->error = ERROR_LAMPI_INIT_AT_FORK;
        return;
    }

    if (s->useDaemon) {
        /* adjust rank to account for deamon process */
        s->local_rank--;
        if (s->local_rank == -1)
            s->iAmDaemon = 1;
        else
			s->IAmAlive[s->local_rank] = 1;            
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
    lampiState.global_rank =
        lampiState.local_rank + lampiState.global_to_local_offset;

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
    if (!s->client->clientConnect(s->local_size, s->hostid)) {
        s->error = ERROR_LAMPI_INIT_CONNECT_TO_MPIRUN;
        return;
    }
#ifdef USE_CT
	// Now wait until network has been linked
	while ( false == s->client->clientNetworkHasLinked() )
		usleep(10);
#endif
}


void lampi_init_prefork_parse_setup_data(lampiState_t *s)
{
    int tag, recvd;
    int pathcnt, *paths;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_msg");
    }
    //ASSERT: adminMessage buffer contains run params
    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->nextTag(&tag);
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
            s->client->unpackMessage(&(s->nhosts),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            s->client->setNumberOfHosts(s->nhosts);
            s->map_host_to_local_size = ulm_new(int, s->nhosts);
            s->client->unpackMessage(s->map_host_to_local_size,
                                     (adminMessage::packType) sizeof(int),
                                     s->nhosts);

            break;
        case adminMessage::CHANNELID:
            /* number of hosts, and number of procs per host */
            s->client->unpackMessage(&(s->channelID),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
        case adminMessage::HOSTID:
            s->client->unpackMessage(&(s->hostid),
                                     (adminMessage::packType) sizeof(int),
                                     1);
			s->client->setHostRank(s->hostid);
            break;
        case adminMessage::TVDEBUG:
            s->client->unpackMessage(&(s->debug),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            s->client->unpackMessage(&(s->debug_location),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
        case adminMessage::THREADUSAGE:
            s->client->unpackMessage(&(s->usethreads),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
        case adminMessage::CRC:
            s->client->unpackMessage(&(s->usecrc),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
        case adminMessage::CHECKARGS:
            s->client->unpackMessage(&lampiState.checkargs,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;

        case adminMessage::OUTPUT_PREFIX:
            s->client->unpackMessage(&(s->output_prefix),
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
#ifdef __mips
        case adminMessage::CPULIST:
            // list of cpus to use
            {
                int c, cpulistlen, *cpulist;
                constraint_info my_c;
                s->client->unpackMessage(&cpulistlen,
                                         (adminMessage::
                                          packType) sizeof(int), 1);

                cpulist = ulm_new(int, cpulistlen);
                s->client->unpackMessage(cpulist,
                                         (adminMessage::
                                          packType) sizeof(int),
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
            s->client->unpackMessage(&nCpPNode,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if ((nCpPNode < 0) || (nCpPNode > 2)) {
                ulm_exit((-1, "Incorrect value for nCpPNode :: %d\n",
                          nCpPNode));
            }
            break;

        case adminMessage::USERESOURCEAFFINITY:
            /* specify if resource affinit is to be used */
            s->client->unpackMessage(&tmpInt,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit((-1,
                          "Incorrect value for USERESOURCEAFFINITY (%d)\n",
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
            s->client->unpackMessage(&tmpInt,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit((-1,
                          "Incorrect value for DEFAULTRESOURCEAFFINITY (%d)\n",
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
            s->client->unpackMessage(&tmpInt,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit((-1,
                          "Incorrect value for MANDATORYRESOURCEAFFINITY (%d)\n",
                          tmpInt));

            }
            if (tmpInt == 1) {
                affinMandatory = true;
            } else {
                affinMandatory = false;
            }
            break;
#endif


        case adminMessage::MAXCOMMUNICATORS:
            /* specify upper limit on communicators */
            s->client->unpackMessage(&maxCommunicatorInstances,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if (maxCommunicatorInstances < 0) {
                ulm_exit((-1,
                          "Incorrect value for MAXCOMMUNICATORS (%d)\n",
                          maxCommunicatorInstances));
            }
            break;

        case adminMessage::NPATHTYPES:
            s->client->unpackMessage(&pathcnt,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            if (pathcnt) {
                paths = ulm_new(int, pathcnt);
                s->client->unpackMessage(paths,
                                         (adminMessage::packType) sizeof(int),
                                         pathcnt);
                for (int i = 0; i < pathcnt; i++) {
                    switch (paths[i]) {
                    case PATH_UDP:
                        s->udp = 1;
                        break;
                    case PATH_GM:
                        s->gm = 1;
                        break;
                    case PATH_QUADRICS:
                        s->quadrics = 1;
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
            s->client->unpackMessage(&maxIRecvDescRetries,
                                     (adminMessage::packType) sizeof(long),
                                     1);
            break;
        case adminMessage::IRCEVMINPAGESPERCTX:
            /* minimum number of pages allocated to each
             *   list of receive descirptors */
            s->client->unpackMessage(&minPgsIn1IRecvDescDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;

        case adminMessage::IRECVMAXPAGESPERCTX:
            /* maximum number of pages allocated to each
             *   list of receive descirptors */
            s->client->unpackMessage(&maxPgsIn1IRecvDescDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;

        default:
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
    }
}


void lampi_init_prefork_receive_setup_msg(lampiState_t *s)
{
    int 	errorCode, h,i,p;
	int		*labelsToRank, tag, rank;
	
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_msg");
    }

	// Get broadcasted setup data.
	rank = -1;
	tag = adminMessage::RUNPARAMS;
    if ( false == s->client->receiveMessage(&rank, &tag, &errorCode) )
	{
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }
    lampi_init_prefork_parse_setup_data(s);
    if (s->error) {
        return;
    }
	
	// Get scattered setup data.
    if ( false == s->client->scatterv(-1, adminMessage::RUNPARAMS, &errorCode) )
	{
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }
    lampi_init_prefork_parse_setup_data(s);
    if (s->error) {
        return;
    }

	/* exchange our node label <-> host rank map. */
	labelsToRank = (int *)ulm_malloc(s->nhosts * sizeof(int));
	if  ( NULL == labelsToRank )
	{
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
	}
	
	s->client->exchange(&(s->hostid), labelsToRank, sizeof(int));        
	//ASSERT: labelsToRank[i] is the host rank of daemon node i.
	s->client->setLabelsToRank(labelsToRank);
	
    /* decide if a daemon will be used */
#ifdef USE_RMS
    s->useDaemon=0;
#else
    s->useDaemon=1;
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

    s->client->synchronize(s->nhosts + 1);

    /*
     * receive path specific information
     */
    lampi_init_prefork_receive_setup_params_shared_memory(s);
    lampi_init_prefork_receive_setup_params_quadrics(s);
    lampi_init_prefork_receive_setup_params_gm(s);

    return;
}


void lampi_init_prefork_receive_setup_params(lampiState_t *s)
{
    int tag, errorCode, recvd, h, i, p;
    int pathcnt, *paths;

#ifdef USE_CT
    lampi_init_prefork_receive_setup_msg(s);
    return;
#endif

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
        case adminMessage::TVDEBUG:
            s->client->unpack(&(s->debug),
                              (adminMessage::packType) sizeof(int), 1);
            s->client->unpack(&(s->debug_location),
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
#ifdef __mips
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
                ulm_exit((-1, "Incorrect value for nCpPNode :: %d\n",
                          nCpPNode));
            }
            break;

        case adminMessage::USERESOURCEAFFINITY:
            /* specify if resource affinit is to be used */
            s->client->unpack(&tmpInt,
                              (adminMessage::packType) sizeof(int), 1);
            if ((tmpInt < 0) || (tmpInt > 1)) {
                ulm_exit((-1,
                          "Incorrect value for USERESOURCEAFFINITY (%d)\n",
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
                ulm_exit((-1,
                          "Incorrect value for DEFAULTRESOURCEAFFINITY (%d)\n",
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
                ulm_exit((-1,
                          "Incorrect value for MANDATORYRESOURCEAFFINITY (%d)\n",
                          tmpInt));

            }
            if (tmpInt == 1) {
                affinMandatory = true;
            } else {
                affinMandatory = false;
            }
            break;
#endif


        case adminMessage::MAXCOMMUNICATORS:
            /* specify upper limit on communicators */
            s->client->unpack(&maxCommunicatorInstances,
                              (adminMessage::packType) sizeof(int), 1);
            if (maxCommunicatorInstances < 0) {
                ulm_exit((-1,
                          "Incorrect value for MAXCOMMUNICATORS (%d)\n",
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
                    case PATH_GM:
                        s->gm = 1;
                        break;
                    case PATH_QUADRICS:
                        s->quadrics = 1;
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
#ifdef USE_RMS
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
    /* as long as mpirun participates in the "wire up", the
     *   order these paths are initialized needs to be mirrored
     *   in mpirun
     */
    lampi_init_prefork_udp(s);
    lampi_init_prefork_quadrics(s);
    lampi_init_prefork_gm(s);

}


void lampi_init_postfork_paths(lampiState_t *s)
{
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
    lampi_init_postfork_quadrics(s);
    lampi_init_postfork_gm(s);
    //lampi_init_postfork_local_coll(s);
}


void lampi_init_wait_for_start_message(lampiState_t *s)
{
#ifndef USE_CT
    int tag, errorCode, goahead, recvd;
    bool r;
#endif

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
#ifdef USE_CT

        s->client->synchronize(s->nhosts + 1);

#else
        r = s->client->reset(adminMessage::SEND);
        r = r && s->client->send(-1, adminMessage::BARRIER, &errorCode);
        r = r
            && ((recvd = s->client->receive(-1, &tag, &errorCode)) ==
                adminMessage::OK);
        r = r && (tag == adminMessage::BARRIER)
            && s->client->unpack(&goahead,
                                 (adminMessage::packType) sizeof(int), 1);
        if (!r || !goahead) {
            s->error = ERROR_LAMPI_INIT_WAIT_FOR_START_MESSAGE;
        }
#endif
    }

    s->client->localBarrier();
}


void lampi_init_prefork_debugger(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_debugger");
    }

    if (!s->debug) {
        return;
    }

    if (s->debug_location == 0) {
        ClientTVSetup();
    }
}


void lampi_init_postfork_debugger(lampiState_t *s)
{
    int errorCode, sendPIDs;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_debugger");
    }

    if (!s->debug) {
        return;
    }

    if (s->debug_location == 1) {
        sendPIDs = 0;
        if ((s->useDaemon && s->iAmDaemon) ||
            (!s->useDaemon && (s->local_rank == 0))) {
            sendPIDs = 1;
            // CLIENTPIDS exchange from local rank 0 process
            s->client->reset(adminMessage::SEND);
            s->client->pack((void *) s->local_pids,
                            (adminMessage::packType) sizeof(pid_t),
                            s->local_size);
#ifdef USE_CT
            s->client->sendMessage(0, adminMessage::CLIENTPIDS,
                                   s->channelID, &errorCode);
#else
            s->client->send(-1, adminMessage::CLIENTPIDS, &errorCode);
#endif
        }

        if (!s->iAmDaemon)
            ClientTVSetup();
    }
}


void lampi_init_prefork_stdio(lampiState_t *s)
{
    int NChildren, TmpFDERR[2], TmpFDOUT[2];
    int i, RetVal;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_stdio");
    }

    /* do nothing if stdio is not being managed */
    if (!s->interceptSTDio)
        return;

    /* establish socket connection for stderr/stdout traffic to mpirun */
    s->STDERRfdToMPIrun = STDERR_FILENO;
    s->STDOUTfdToMPIrun = STDOUT_FILENO;

    /*
     * dup current stderr and stdout so that Client's stderr and
     * stdout can be restored to those before exiting this routines.
     */

    dupSTDERRfd = dup(STDERR_FILENO);
    if (dupSTDERRfd <= 0) {
        ulm_exit((-1, "Error: duping STDERR_FILENO.\n"));
    }
    dupSTDOUTfd = dup(STDOUT_FILENO);
    if (dupSTDOUTfd <= 0) {
        ulm_exit((-1, "Error: duping STDOUT_FILENO.\n"));
    }

    /*
     * get count for number of pipes needed between the daemon process
     * and the children.
     */
    NChildren = s->local_size;

    /* setup stdio/stderr redirection */
    StderrPipes = ulm_new(int, 2 * NChildren);
    for (i = 0; i < NChildren; i++) {
        if (pipe(StderrPipes + (2 * i)) < 0) {
            ulm_exit((-1, "Error: opeing pipe.  Errno %d", errno));
        }
    }

    StdoutPipes = ulm_new(int, 2 * NChildren);
    for (i = 0; i < NChildren; i++) {
        if (pipe(StdoutPipes + (2 * i)) < 0) {
            ulm_exit((-1, "Error: opeing pipe.  Errno %d", errno));
        }
    }
    /* create a pipe for stderr from the master to mpirun */
    RetVal = pipe(TmpFDERR);
    if (RetVal < 0) {
        ulm_exit((-1, "Unable to open pipe.  Errno : %d\n", errno));
    }

    s->STDERRfdsFromChildren = ulm_new(int, NChildren + 1);
    fflush(stderr);
    s->STDERRfdsFromChildren[NChildren] = TmpFDERR[0];
    RetVal = dup2(TmpFDERR[1], STDERR_FILENO);
    if (RetVal < 0) {
        ulm_exit((-1,
                  "Unable to dup STDERRfdsFromChildren[NChildren] : %d",
                  s->STDERRfdsFromChildren[NChildren]));
    }

    /* create a pipe for stdout from the master to mpirun */
    RetVal = pipe(TmpFDOUT);
    if (RetVal < 0) {
        ulm_exit((-1, "Unable to open pipe.  Errno : %d\n", errno));
    }
    s->STDOUTfdsFromChildren = ulm_new(int, NChildren + 1);
    fflush(stdout);
    s->STDOUTfdsFromChildren[NChildren] = TmpFDOUT[0];
    RetVal = dup2(TmpFDOUT[1], STDOUT_FILENO);
    if (RetVal < 0) {
        ulm_exit((-1,
                  "Unable to dup STDOUTfdsFromChildren[NChildren] : %d",
                  s->STDOUTfdsFromChildren[NChildren]));
    }

    /*
     * setup array holding prefix data for stdio data coming from
     * "worker" processes
     */

    s->IOPreFix = ulm_new(PrefixName_t, NChildren + 1);
    s->LenIOPreFix = ulm_new(int, NChildren + 1);
    s->NewLineLast = ulm_new(int, NChildren + 1);
    for (i = 0; i <= NChildren; i++) {
        /* null prefix */
        s->IOPreFix[i][0] = '\0';
        s->LenIOPreFix[i] = 0;
        s->NewLineLast[i] = 1;
    }
    if (s->output_prefix) {
        /* activate prefix */
        for (i = 0; i < NChildren; i++) {
            sprintf(s->IOPreFix[i], "%d[%d.%d] ",
                    i + s->global_to_local_offset, s->hostid, i);
            s->LenIOPreFix[i] = (int) strlen(s->IOPreFix[i]);
        }
        sprintf(s->IOPreFix[NChildren], "daemon[%d] ", s->hostid);
        s->LenIOPreFix[NChildren] = (int) strlen(s->IOPreFix[NChildren]);
    }
    s->StderrBytesWritten = 0;
    s->StdoutBytesWritten = 0;
}


void lampi_init_postfork_stdio(lampiState_t *s)
{
    int NChildren, i, RetVal;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_stdio");
    }

    /* do nothing if stdio is not being managed */
    if (!s->interceptSTDio)
        return;

    NChildren = s->local_size;

    /* Client clean up */
    if (s->iAmDaemon) {
        /* close all write stderr/stdout pipe fd's ) */
        for (i = 0; i < NChildren; i++) {
            close(StderrPipes[2 * i + 1]);
            close(StdoutPipes[2 * i + 1]);
            s->STDERRfdsFromChildren[i] = StderrPipes[2 * i];
            s->STDOUTfdsFromChildren[i] = StdoutPipes[2 * i];
        }
        /* restore STDERR_FILENO and STDOUT_FILENO to state when
         * this routine was entered */
        fflush(stderr);
        //s->StderrFD=dupSTDERRfd;
        RetVal = dup2(dupSTDERRfd, STDERR_FILENO);
        if (RetVal <= 0) {
            ulm_exit((-1, "Error: in dup2 dupSTDERRfd, STDERR_FILENO.\n"));
        }
        fflush(stdout);
        //s->StdoutFD=dupSTDOUTfd;
        RetVal = dup2(dupSTDOUTfd, STDOUT_FILENO);
        if (RetVal <= 0) {
            ulm_exit((-1, "Error: in dup2 dupSTDOUTfd, STDOUT_FILENO.\n"));
        }
        /* end of daemon code */
    } else {
        /* setup "application process" handling of stdout/stderr */
        dup2(StderrPipes[2 * lampiState.local_rank + 1],
             STDERR_FILENO);
        dup2(StdoutPipes[2 * lampiState.local_rank + 1],
             STDOUT_FILENO);
        for (i = 0; i < NChildren; i++) {
            /* close read side of pipe on child */
            if (i == s->local_rank) {
                close(StderrPipes[2 * s->local_rank]);
                close(StdoutPipes[2 * s->local_rank]);
            } else {
                /* close pipes set up for other processes */
                close(StderrPipes[2 * i]);
                close(StderrPipes[2 * i + 1]);
                close(StdoutPipes[2 * i]);
                close(StdoutPipes[2 * i + 1]);
            }
        }                       /* end i loop */
    }

    ulm_delete(StderrPipes);
    ulm_delete(StdoutPipes);
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
    s->debug = 0;
    s->interceptSTDio = 1;

    /* network path types */
    s->quadrics = false;
    s->gm = 0;
    s->udp = 0;


    /* MPI buffered send data */
    s->bsendData = (bsendData_t *) ulm_malloc((sizeof(bsendData_t)));
    if (!s->bsendData) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }

    /* Context ID allocation control */
    s->contextIDCtl = (contextIDCtl_t *) ulm_malloc((sizeof(contextIDCtl_t)));
    if (!s->bsendData) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
        return;
    }
    memset(s->contextIDCtl, 0, sizeof(contextIDCtl_t));
}
