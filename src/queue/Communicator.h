/*
 * Copyright 2002-2003. The Regents of the University of
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

#ifndef QUEUES_COMMUNICATOR_H
#define QUEUES_COMMUNICATOR_H

#include "os/atomic.h"

#define ULM_64BIT_INT		long long
#define HWSPECIFICBARRIER        1
#define SMPSWBARRIER             2

#include "internal/ftoc.h"
#include "path/common/BaseDesc.h"
#include "queue/Group.h"
#include "sender_ackinfo.h"
#include "ulm/ulm.h"
#include "internal/new.h"
#include "util/DblLinkList.h"
#include "util/Lock.h"

#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPFragDesc.h"
#include "queue/SharedMemForCollective.h"
#endif

#ifdef USE_ELAN_COLL
#include "path/quadrics/coll.h"
#define BCAST_DESC_POOL_SZ		32
#endif

enum { MCAST_BUF_SZ = 16384 };

struct ulm_comm_info_t {
    int myGlobalRank;           // info for ulm
    int numRanksOnHost;         // unique for this host
    int *onHostRankTable;
    int commSize;               // number of ranks in COMM
    int *listOfRanksInComm;     // info for ulm (L2Gtable)
    int numHostsInComm;         // define the len. of hostInfo array
    ulm_host_info_t *hostInfo;  // array of struct defined above
};

/*
 * attributes
 */
struct attributes_t {
    //Lock
    Locks Lock;
    // flag indicating usage
    int inUse;
    // is this set from fortran ?
    int setFromFortran;
    // copy function - C
    MPI_Copy_function *cCopyFunction;
    // copy function - Fortran
    Fortran_Copy_function *fCopyFunction;
    // delete function - C
    MPI_Delete_function *cDeleteFunction;
    // delete function - Fortran
    Fortran_Delete_function *fDeleteFunction;
    // extra state
    void *extraState;
    // marked for deletion ?
    int markedForDeletion;
    // reference count
    int refCount;
};

struct commAttribute_t {
    int keyval;
    void *attributeValue;
};

// pool of attributes_t and control information needed to manage it
struct attributePool_t {

    // lock to control access to pool
    Locks poolLock;

    // size of pool
    int poolSize;

    // number of elements in use
    int eleInUse;

    // first element available
    int firstFreeElement;

    // array of elements
    attributes_t *attributes;
};

// process private message queues
struct procPrivateQs_t {
    // receive side
    // posted receive messages, wild source process
    ProcessPrivateMemDblLinkList PostedWildRecv;

    // posted receive messages, specified source process
    // sorted based on source process
    ProcessPrivateMemDblLinkList **PostedSpecificRecv;

    // matched posted receives, source process specified
    // sorted based on source process
    ProcessPrivateMemDblLinkList **MatchedRecv;

    // received message frags that can't be matched yet
    // sorted based on source process
    ProcessPrivateMemDblLinkList **AheadOfSeqRecvFrags;

    // received message frags that may be used to match
    //   posted receives
    // sorted based on source process
    ProcessPrivateMemDblLinkList **OkToMatchRecvFrags;

#ifdef ENABLE_SHARED_MEMORY
    ProcessPrivateMemDblLinkList **OkToMatchSMPFrags;
#endif                          // SHARED_MEMORY

};


// data structure for caching data for collective's optimization

struct list_t {
    int length;
    int *list;
};

struct CollectiveOpt_t {
    // send/recv pattern - only for larest power of 2 hosts that fits within
    //   the current set of 2
    list_t *hostExchangeList;

    // number of hosts about the largest power of 2 in the current host
    //   group
    int nExtra;

    // number of hosts in the largest power of 2 host set
    int extraOffset;

    // number of pair-wise exchanges the tree describes
    int treeDepth;

    // the order in which the data will be arrive from the remote hosts
    int *dataHostOrder;

    // array to hold he amount of data that each host will send around
    //   and the number of stripes for this collective
    size_t *dataToSend;

    // array to hold the amount of data that each host sends in
    //   the current stripe - this is not a running sum
    size_t *dataToSendThisStripe;

    // array to keep track of which local process's data is being read in
    int *currentLocalRank;

    // array to keep track of the data offset into the current local
    //   process's data that is in the shared memory buffer.
    size_t *currentRankBytesRead;

    // array indicating how much data to send this time around
    size_t *dataToSendNow;

    // set up array indicating how much data to receive this time around
    size_t *recvScratchSpace;

    // size of shared memory buffer allocated for the local host
    ssize_t localStripeSize;

    // size of shared memory buffer allocated per rank
    ssize_t perRankStripeSize;

    // shared buffer offset used for reduce on this host, and the
    // minimum such offset across all hosts, and the max extent for
    // which the shared memory buffer can be used
    size_t reduceOffset;
    size_t minReduceOffset;
    size_t maxReduceExtent;
};


// structure to store a pointer to a group object, and information
// needed to manage it's use.
struct sharedQs_t {
    volatile int localRootProcess;
    volatile int contextID;
    volatile bool InUse;
    volatile int nAllocated;
    volatile int nFreed;
};


class Communicator {
private:
    // tag for use with collective operations
    long long base_tag;

    // lock for use in guaranteeing unique tags
    Locks base_tag_lock;

    // multicast vpid for quadrics hw bcast 
    int *multicast_vpid;

    // buffer for use with hardware multicast (per rail)
    unsigned char **elan_mcast_buf;

public:

/*
 *   MEMBER VARIABLES
 */

    struct {
        ulm_allgather_t *allgather;
        ulm_allgatherv_t *allgatherv;
        ulm_allreduce_t *allreduce;
        ulm_alltoall_t *alltoall;
        ulm_alltoallv_t *alltoallv;
        ulm_alltoallw_t *alltoallw;
        ulm_barrier_t *barrier;
        ulm_bcast_t *bcast;
        ulm_gather_t *gather;
        ulm_gatherv_t *gatherv;
        ulm_reduce_t *reduce;
        ulm_reduce_scatter_t *reduce_scatter;
        ulm_scan_t *scan;
        ulm_scatter_t *scatter;
        ulm_scatterv_t *scatterv;
    } collective;

    // topology
    ULMTopology_t *topology;

    // hardware quadrics context for multicast
    int hw_ctx_stripe;

    // attributes
    int attributeCount;
    int sizeOfAttributeArray;
    commAttribute_t *attributeList;
    enum { ATTRIBUTE_CACHE_GROWTH_INCREMENT = 5 };

    // reference count
    int refCount;
    // number of requests referencing this communicator
    int requestRefCount;
    // lock to protect integrity of above reference counts
    Locks refCounLock;

    // enum for indicating communicator type
    enum { INTRA_COMMUNICATOR, INTER_COMMUNICATOR };
    enum { MAX_COMM_CACHE_SIZE = 5 };
    enum { INTERCOMM_TAG = -3 };	// intercomm_merge

    // communicator type
    int communicatorType;

    // ok to delete communicator
    int markedForDeletion;

    // local group's communicator - for inter communicators
    int localGroupsComm;

    // can communicator be freed ?
    bool canFreeCommunicator;

    // generic lock for infrequent operations
    Locks commLock;

    // index for error handler table
    int errorHandlerIndex;

    // group pointers - for intra communicators these will be identicle
    //   and for inter communicators, these will be distinct.
    Group *localGroup;
    Group *remoteGroup;

    // communicator index
    int contextID;

    // communicator cache - used for forming new groups
    int cacheSize;
    int *commCache;
    int availInCommCache;

    /* structures for collective alrogithm optimization */
    int useSharedMemForCollectives;

    /* next element in pool to use in next collective op */
    int collectivePoolIndex;

    /* element in SharedMemForCollective being used */
    int collectiveIndexDesc;
    int collectiveInUse;


    /* pointer to pool allocated to this communicator */
    CollectiveSMBuffer_t *sharedCollectiveData;

    // get new contextID - from the cache (may need to refresh the cache)
    int getNewContextID(int *outputContextID);

    // is threaded access expected for this group
    bool useThreads;

    // next pt-2-pt message sequence number expected on the receive
    // side process private
    ULM_64BIT_INT *next_expected_isendSeqs;
    Locks *next_expected_isendSeqsLock;

    // posted recv counter - used to keep track internally of
    //  the posted receives
    bigAtomicUnsignedInt next_irecv_id_counter;
    Locks next_irecv_id_counterLock;

    // point-to-point receive lock. To avoid a race conditions between
    // a receive being posted and an incoming frament being placed on
    // the privateQueues.OkToMatchRecvFrags list, this lock must be
    // aquired before posting a receive of processing an incoming
    // fratment.  Alternatively one could keep checking the posted
    // receive queues and the privateQueues.OkToMatchRecvFrags queues,
    // but this seems more expensive.
    Locks *recvLock;

    // next pt-2-pt message sequence number generated on the send side
    //    process private
    ULM_64BIT_INT *next_isendSeqs;
    Locks *next_isendSeqsLock;

    // see if shared memory queues are actually allocated from shared
    // memory (for COMM_SELF this is not the case)
    bool shareMemQueuesUsed;
    // index of shared memory queues in queue pool
    int indexSharedMemoryQueue;

    procPrivateQs_t privateQueues;

    enum {
        SPECIFIC_RECV_QUEUE,    // queue of specific receives
        WILD_RECV_QUEUE         // queue of wild receives
    };

    // on host barrier type
    int SMPBarrierType;

    // pointer to barrier data structure - an optimization (barrierControl also
    //   holds the pointer as one of it's elements)
    volatile void *barrierData;

    // pointer to barrier control structure
    void *barrierControl;

    // pointer to buffer for reduce
    void *reduceBuffer;
    size_t reduceBufferSize;

    
/*
 *   MEMBER METHODS
 */
    
public:
        

    int updateCache(int baseValue, int len) {
        if (len > cacheSize)
            return ULM_ERR_BAD_PARAM;

        for (int i = 0; i < len; i++)
            commCache[i] = baseValue + i;
        availInCommCache = len;

        return ULM_SUCCESS;
    }

    int initializeCollectives(int useSharedMem);

    /* structure for caching data for collective's optimizations */
    CollectiveOpt_t collectiveOpt;

    void freeCollectivesResources();

    // Fetch the multicast vpid associated with this communicator/rail
    // when running under osf.  Returns -1 if there is no such vpid
    int getMcastVpid(int rail, int *vp);

    // Fetch the multicast buffer associated with this 
    // communicator/rail when running under osf.  Returns 0 if there
    // is no such buffer.  The size of the buffer, if it exists, is
    // stored in the sz pointer.
    void* getMcastBuf(int rail, size_t *sz);

    /*
     * get pointer to collective descriptor
     */
    CollectiveSMBuffer_t *getCollectiveSMBuffer(long long tag,
                                                int collectiveType =
                                                ULM_COLLECTIVE_ALLGATHER) {
        CollectiveSMBuffer_t *smbuf;

        collectiveInUse = collectiveIndexDesc;
        collectiveIndexDesc++;
        if (collectiveIndexDesc == N_COLLCTL_PER_COMM)
            collectiveIndexDesc = 0;

        smbuf = &(sharedCollectiveData[collectiveInUse]);
        return smbuf;
    }

    /*
     * release  pointer to collective descriptor
     */
    void releaseCollectiveSMBuffer(CollectiveSMBuffer_t *smbuf) {
        /* if we are recycling, reinitialize the flags that all use */
        if( collectiveIndexDesc == 0 ) {
            /* make sure all are done with the data */
            smpBarrier(barrierData);
            if( localGroup->onHostProcID == 0 ) {
		for( int ele=0 ; ele <  N_COLLCTL_PER_COMM ; ele++ ) {
                    sharedCollectiveData[ele].flag=0;
		}
            }
            /* don't let anyone move ahead until initialization is done */
            smpBarrier(barrierData);
        }
    } // end releaseCollectiveSMBuffer

    // function pointer to bind point-to-point messages to a path object
    int (*pt2ptPathSelectionFunction) (void **, int, int);

    // constructor
    Communicator() {}

    // destructor
    ~Communicator() {}


    // initialization function - can be used to recycle
    //   an instance of the class
    //  - initialize next_expected_isendSeqs
    //  - initialize next_isendSeqs
    //  - initialize send/recv queues
    //  - set thread usage
    //  - get map of Group to Global ProcID
    int init(int ctxID, bool threadUsage, int group1Index, int group2Index,
             bool firstInstanceOfContext, bool setCtxCache, int sizeCtxCache,
             int *lstCtxElements, int commType, bool okToDeleteComm,
             int useSharedMem, int useSharedMemCollOpts);

    // free process private resources
    int freeCommunicator();

    // retrieve a unique base tag for collective ops must specify, as
    // an argument, the number of tags the operation will use
    long long get_base_tag(int num_requested) {
        long long ret_tag;
        if (num_requested < 1)
            return -1;
        base_tag_lock.lock();
        ret_tag = base_tag;
        base_tag -= num_requested;
        base_tag_lock.unlock();
        return ret_tag;
    }

    // process frags arriving "off the wire"
    int handleReceivedFrag(BaseRecvFragDesc_t *recvFrag, double timeNow = -1.0);
    RecvDesc_t* matchReceivedFrag(BaseRecvFragDesc_t *recvFrag, double timeNow = -1.0);

    //
    // message Queues
    //

    // check to see if queues are empty
    bool areQueuesEmpty();

    // set up the shared memory queues in process private memory
    int setupSharedMemoryQueuesInPrivateMemory();

    //
    // receive functions
    //

    // start non-blocking irecv
    int irecv_start(ULMRequest_t *request);

    //
    // send functions
    //

    // method to start the send using the request object created
    // by the init method
    int isend_start(SendDesc_t **SendDesc);

    // check to see if there is any data to be received
    int iprobe(int sourceProc, int tag, int *found, ULMStatus_t *status);

#ifdef USE_ELAN_COLL
    /* 
     * The list of qsnet hw broadcast operations,
     * may appear completed to the ULM, but SRL is still working on them.
     */
    struct {
      bcast_request_t* handle;
      bcast_request_t* first;
      bcast_request_t* last;
      bcast_request_t* first_free;
    } bcast_queue;

    void  init_bcast_queue()
    {
      bcast_queue.first_free = bcast_queue.handle =
	  (bcast_request_t*) ulm_new(bcast_request_t, BCAST_DESC_POOL_SZ);

      bzero(bcast_queue.handle, BCAST_DESC_POOL_SZ * sizeof(bcast_request_t));

      bcast_queue.first  = bcast_queue.last = (bcast_request_t*)NULL;

      if (! bcast_queue.first_free)
	ulm_err(("Error in allocating memory for bcast requests\n"));
      else
      {
	bcast_request_t * temp_req = bcast_queue.first_free;
	for ( int k = 1 ; k < BCAST_DESC_POOL_SZ; k++)
	{
	  temp_req->next = bcast_queue.first_free + k;
	  (bcast_queue.first_free + k)->prev = temp_req;
	  temp_req = (bcast_request_t*)temp_req->next;
	}
	/* No more elements after this */
	temp_req->next = bcast_queue.first_free ;
	bcast_queue.first_free->prev = temp_req;
      }
    }

    bcast_request_t *  commit_bcast_req(bcast_request_t* bcast_desc)
    {
      assert (bcast_desc == bcast_queue.first_free);

      bcast_queue.first_free = 
	(bcast_request_t *)bcast_desc->next;

      /* Remove the desc from the link list */
      bcast_desc->prev->next = bcast_desc->next;
      bcast_desc->next->prev = bcast_desc->prev;

      /* Commit the descriptor */
      if ( bcast_queue.first == (bcast_request_t*)NULL )
      {
	bcast_queue.first = bcast_queue.last  = bcast_desc;
      }
      else
      {
	bcast_queue.last->next = bcast_desc;
	bcast_queue.last = bcast_desc;
      }
      bcast_desc->next = (bcast_request_t *) NULL;
      bcast_desc->prev = (bcast_request_t *) NULL;
    }
 
    void  free_bcast_req(bcast_request_t* bcast_desc)
    {
      assert ( bcast_queue.first == bcast_desc);

      /* remove from the used list */
      bcast_queue.first = (bcast_request_t*)bcast_desc->next;
      if (bcast_desc == bcast_queue.last)
	bcast_queue.last = (bcast_request_t *) NULL;

      /* return to the free list */
      bcast_queue.first_free->prev->next = bcast_desc;
      bcast_desc->prev = bcast_queue.first_free->prev;

      bcast_queue.first_free->prev = bcast_desc;
      bcast_desc->next = bcast_queue.first_free;
    }

    /* The transporter exported from SRL */
    Broadcaster        *bcaster;

    volatile int        hw_bcast_enabled;   /* if hw_bcast is available */

    /* 
     * Bind a bcast request to a channel.
     * May trigger progress or fail-over 
     */
    int                bcast_bind( bcast_request_t *, int, int, int, 
	void *, size_t , ULMType_t *, int, int, 
	CollectiveSMBuffer_t *, int );

    /* 
     * Wait on a bcast request to complete, 
     * May trigger progress or fail_over 
     */
    int                ibcast_start(bcast_request_t *);
    int                bcast_wait  (bcast_request_t *);
    int                fail_over   (bcast_request_t *);

#endif

    //
    // message matching functions
    //

    // frag processing
    // search for missing frags (old: got_missing_frag)
    RecvDesc_t *isThisMissingFrag(BaseRecvFragDesc_t *rec);

    // search for specified frags
    void SearchForFragsWithSpecifiedISendSeqNum(RecvDesc_t *MatchedPostedRecvHeader,
                                                bool *recvDone, double timeNow = -1.0);

    // search for frags with specified tag
    void SearchForFragsWithSpecifiedTag(RecvDesc_t *MatchedPostedRecvHeader, 
                                        bool *recvDone, double timeNow = -1.0);

    // search ahead of sequence queue to see if frags can now be
    // matched (old name: scan_ahead_of_sequence)
    int matchFragsInAheadOfSequenceList(int proc, double timeNow = -1.0);

    // try and match frag (old name: match_message)
    RecvDesc_t *checkPostedRecvListForMatch(BaseRecvFragDesc_t *RecvDesc);

    // try and match frag when only wild receives are posted
    //  (old name: only_wild)
    RecvDesc_t *checkWildPostedRecvListForMatch(BaseRecvFragDesc_t *rec);

    // try and match frag when only specific (with refernece
    //   to source process) receives are posted
    //  (old name: only_specific)
    RecvDesc_t *checkSpecificPostedRecvListForMatch(BaseRecvFragDesc_t *rec);

    // try and match frag when both specific and wild receives are
    // posted (old name: both_wild_and_specific)
    RecvDesc_t *checkSpecificAndWildPostedRecvListForMatch(BaseRecvFragDesc_t *rec);

#ifdef ENABLE_SHARED_MEMORY
    // special code for on host messaging
    RecvDesc_t *checkSMPRecvListForMatch(SMPFragDesc_t * RecvDesc, int *queueMatched);
    RecvDesc_t *checkSMPWildRecvListForMatch(SMPFragDesc_t * incomingFrag);
    RecvDesc_t *checkSMPSpecificRecvListForMatch(SMPFragDesc_t * incomingFrag);
    RecvDesc_t *checkSMPSpecificAndWildRecvListForMatch
        (SMPFragDesc_t * incomingFrag, int *queueMatched);
#endif

    //
    // posted recv processing
    //

    // try and match posted irecv with wild source and posted message
    // descriptor IRDesc (old name: wild_irecv)
    void checkFragListsForWildMatch(RecvDesc_t *IRDesc);

    // try and match posted irecv with spedified source and posted
    // message descriptor IRDesc (old name: specific_irecv)
    void checkFragListsForSpecificMatch(RecvDesc_t *IRDesc);

    // try and match posted irecv with frag in ProcWithData list
    // and posted message descriptor IRDesc (old name:
    // irecv_find_match)
    bool checkSpecifiedFragListsForMatch(RecvDesc_t *IRDesc, int ProcWithData);

#ifdef ENABLE_SHARED_MEMORY
    bool matchAgainstSMPFragList(RecvDesc_t *receiver, int sourceProcess);
#endif                          // SHARED_MEMORY

    //
    // progess functions
    //

    // on recv side - invoked after match is made
    void ProcessMatchedData(RecvDesc_t *MatchedPostedRecvHeader,
                            BaseRecvFragDesc_t *DataHeader, double timeNow =
                            -1.0, bool * recvDone = 0);

    //
    // Synchronization functions
    //

    // barrier initialization code
    int barrierInit(bool firstInstanceOfContext);
    int platformBarrierSetup(int *callGenericSMPSetup,
                             int *callGenericNetworkSetup, int *gotSMPresouces,
                             int *gotNetworkResources);
    int genericSMPBarrierSetup(int firstInstanceOfContext, int *gotSMPResources);

    // barrier free - host specific
    int barrierFree();
    void freePlatformSpecificBarrier();

    // function pointer to on host Barrier
    void (*smpBarrier) (volatile void *barrierData);
};

#endif
