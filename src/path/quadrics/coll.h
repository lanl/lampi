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


#ifndef QUADRICS_COLL_H
#define QUADRICS_COLL_H

#include <elan3/elan3.h>
#include "path/common/BaseDesc.h"

/*#ifdef 1*/
#ifdef YUW_DBG             /* If debug is enabled. --Weikuan */

#ifndef YUW_DBG_LEVEL
#define YUW_DBG_LEVEL  0
#endif

#define START_MARK  \
do { printf("VP %4d: ", myproc());             \
  printf("Enter %-10s %s \n", __FUNCTION__, __FILE__); fflush(stdout);} \
while (0)

#define END_MARK                                                        \
do { printf("VP %4d: ", myproc());                                      \
  printf("END %-10s %s %d\n", __FUNCTION__, __FILE__, __LINE__);        \
  fflush(stdout);}                                                      \
while (0)

#define YUW_MARK                                                        \
do { printf("VP %4d: ", myproc());                                      \
  printf("%-10s %d \n", __FUNCTION__, __LINE__); fflush(stdout);}       \
while (0)

#else                     /* If debug is not needed */
#define START_MARK  
#define END_MARK  
#define YUW_MARK 
#endif                    /* End of ! YUW_DBG */

#ifdef USE_ELAN_COLL              /* Start of USE_ELAN_COLL. --Weikuan */

/*#define YUW_MAX(a,b)                (((a)>(b))?(a):(b))*/

/* Define a type for virtual memory address in the main memory */
typedef void     * maddr_vm_t ;

#ifndef ELAN_ADDR
typedef unsigned int    ELAN_ADDR;
#endif

#ifndef ELAN_ADDR_NULL
#define ELAN_ADDR_NULL 0
#endif

#ifndef ELAN_ALIGNUP
#define ELAN_ALIGNUP(x,a)       (((uintptr_t)(x) + ((a)-1)) & (-(a)))
                                /* 'a' power of 2 */
#endif

#define M2E(M)  (E3_Addr) elan3_main2elan(ctx, (caddr_t) (M))
#define S2E(S)  (elan3_sdram2elan(ctx, ctx->sdram, (S)))
#define BYTES_FOR_ALIGN(num, align) ((align) - ((num ) % (align )))
#define ALLOC_ALIGN                      (64)
                                         
#define QUADRICS_GLOB_BUFF_POOLS         (8)
#define SYNC_BRANCH_RATIO                (4)
#define MAX_NO_RAILS                     (2)

/* Number of memory buffer pools quadrics is supporting */
#define BCAST_INLINE_SIZE_SMALL          (16)
#define BCAST_INLINE_SIZE                (80)
#define BCAST_SMALLMESG                  (16384)
#define MAX_BCAST_FRAGS                  (2)
                                         
#define BCAST_RECV_TIMEOUT               (4)
#define BCAST_SEND_TIMEOUT               (4)
#define BCAST_SYNC_TIMEOUT               (8)
#define BCAST_RAIL_TIMEOUT               (16)

/* Number of entries for the colletive descriptor queue */
#define QUADRICS_NUM_COLL_ENTRIES        (16 )
#define	BCAST_CHANNEL_LENGTH             (128 * 1024)
#define E3_DMA_SDRAM_CUTOFF              (128)
#define CONTROL_MAIN                     (4096 )
#define QUADRICS_GLOB_MEM_MAIN_POOL_SIZE (2 * 1024 * 1024)

/* This defines the maximal non-blocking collective in progress */
#define QUADRICS_GLOB_MEM_ELAN_POOL_SIZE (64 * 4 * 32) 

/* Global main memory of 16M, supporting 8 pools of 2M each */
#define QUADRICS_GLOB_MEM_MAIN   ( QUADRICS_GLOB_BUFF_POOLS * \
    (QUADRICS_GLOB_MEM_MAIN_POOL_SIZE + CONTROL_MAIN))

/* Global Elan memory for collective dma structures */
#define QUADRICS_GLOB_MEM_ELAN   \
  ( QUADRICS_GLOB_BUFF_POOLS * QUADRICS_GLOB_MEM_ELAN_POOL_SIZE)

#define E3_DMA_REVB_BUG_2(SIZE, ADDR, PAGESIZE)           \
( (((int) (ADDR) & (PAGESIZE-64)) == (PAGESIZE-64)) && (-(((int) (ADDR) | ~(PAGESIZE-1))) < (SIZE)) )

#define ELAN3_ADDRESSABLE(ctx, addr, size)       \
  ((elan3_main2elan(ctx, (char*)addr) != ELAN_BAD_ADDR) &&      \
  (elan3_main2elan(ctx, (char*)addr + size - 1) != ELAN_BAD_ADDR))


enum 
{ 
  BCAST_IDLE_CHANNEL=0,              /* Channel is idle */
  BCAST_SEND_CHANNEL=1,              /* A send channel  */
  BCAST_BUFF_CHANNEL=2,              /* A send channel with buffered data*/
  BCAST_RECV_CHANNEL=3,              /* A recv channel  */
  BCAST_USMP_CHANNEL=4,              /* A channel for a process on SMP */
  BCAST_COMP_CHANNEL=5,              /* A channel just done using */
  BCAST_CHANNEL_TYPES
};

enum
{
  BCAST_MESG_TYPE_INVALID  = 0,
  BCAST_MESG_TYPE_INLINE_S = 1,
  BCAST_MESG_TYPE_INLINE_L = 2,
  BCAST_MESG_TYPE_SHORT    = 3,
  BCAST_MESG_TYPE_LONG     = 4,
  BCAST_MESG_TYPE_BUGGY    = 5,
  BCAST_MESG_TYPE_SYNC     = 6,
  BCAST_MESG_TYPE_NACK     = 7,
  BCAST_NUM_COLL_MESG_TYPE
};

enum {
       NONE_COLL       = 0x0000,
       QSNET_COLL      = 0x0001,
       IBA_COLL        = 0x0002,
       BLUEGENE_COLL   = 0x0004, 
       MAX_COLL        = 3
};

/* 
 * The envelope of this collective request,
 * This should contain enough information about this operation
 * This also must be <= 56 bytes, so that it can be 
 * contained in a block transfer.
 *
 * When the receiver detects the arrival of the message, it also
 * knows what the checksum is. The checksum is 32 bits long.
 *
 */

typedef struct ulm_coll_env
{
  /* 16 bytes */
  int                  coll_type;
  int                  comm_index;
  int                  group_index;
  int                  desc_index;

  /* 16 bytes */
  int                  root;
  int                  tag_m;	
  int                  key;	
  int                  checksum;		
} 
ulm_coll_env_t;

#define      HEADER_COMMON                                                 \
  /* 16 bytes */                                                           \
  int                  coll_type;                                          \
  int                  comm_index;                                         \
  int                  group_index;                                        \
  int                  desc_index;                                         \
                                                                           \
  /* 8 bytes */                                                            \
  int                  root;                                               \
  int                  tag_m;	                                           \
                                                                           \
  /* 16 bytes */                                                           \
  int                  key;                                                \
  int                  mesg_length;    /* The length of contiguous data*/  \
  int                  frag_length;    /* The length of the frag       */  \
  int                  data_length    /* The length of the dma        */   \


typedef struct quadrics_short_header
{
  HEADER_COMMON;

  /* 16 bytes */
  unsigned char        inline_data[16];  

  int                  data_checksum;  /* crc/checksum for the data    */
  int                  checksum;       /* the checksum for the header  */		
  int                  data[0];        /* The start of real data */
} 
short_header_t;

typedef struct quadrics_long_header
{
  HEADER_COMMON;
  /* 16 bytes */
  unsigned char        inline_data[BCAST_INLINE_SIZE];  

  int                  data_checksum;  /* crc/checksum for the data    */
  int                  checksum;       /* the checksum for the header  */		
  int                  data[0];        /* The start of real data */
} 
long_header_t;

typedef union quadrics_coll_header
{
  /* Many fields are not used, reserve them for later optimization */
  short_header_t      sform;
  long_header_t       lform;
}
quadrics_coll_header_t;

/* 
 * Control structure for a collective operation, 
 * These does not have to be in global elan addressable memory
 */

typedef struct quadrics_channel
{
    /* link back to the actual request */
    bcast_request_t   *ulm_request; 

    /* 
     * Since there are no collective ulm_request,
     * Some information needs to be recorded here
     */
    int                comm;

    /* Could be free, inited, started, acked, completed. --Weikuan */ 
    int                status;
    unsigned int       tag;
    int                toack;          /* acknowledgement needed ?*/
    int                crc;            /* checksum needed        ?*/

    ULMType_t         *data_type;	
    int                comm_type;       /* type of the collective */

    int                root;           /* The root process */

    int                count;
    int	               seqno;
    int	               index;          /* index in the queue */
    int	               send_mode;

#ifdef RELIABILITY_ON
    /* Retransmission Stuff */
    uint64_t           time_started;   /* Use elan_clock */
    int                repeats;        /* Number of repeats */
#endif

    /* memory locations */
    maddr_vm_t         appl_addr; 
    /*maddr_vm_t         recv_buff;*/
    maddr_vm_t         mcast_buff;
    maddr_vm_t         offset;          /* where the real data starts  */
}
quadrics_channel_t;

typedef struct quadricsGlobInfo
{
    /* The elan context for the collectives */
    ELAN3_CTX         *ctx;
    ELAN3_CTX         **allCtxs;

    /* The elan global info for the process */
    int                dmaType;
    int                waitType;
    int                retryCount;

    int                nrails;
    int                nvp;
    int                self;   /* elan vp */

    int                nhosts;
    int                hostid;

    int                nlocals;
    int                maxlocals;
    int                localId;
    int                globalId;

    ELAN_LOCATION      myloc;
    ELAN_CAPABILITY   *cap;    /* keep a pointer to the global cap*/

    /* The initially allocate global memory */
    maddr_vm_t         globMainMem;

    /* The initially allocate global elan memory, one per rail */
    sdramaddr_t        globElanMem;
    int                globMainMem_len;
    int                globElanMem_len;
} quadricsGlobInfo_t;

/* The following is just a scaffold for now, not used as yet */
typedef struct quadrics_global_vm_pool
{
  int                  pool_index;
                      
  /* the number of communicators referring this pool */
  int                  ref_count; 
                       
  void            *    base_main;
  void            *    base_elan;
                       
  void            *    start_main;
  void            *    start_elan;
  void            *    current_main;
  void            *    current_elan;
                       
  int                  length_main;
  int                  length_elan;
  int                  chunk_size_main;
  int                  chunk_size_elan;
  int                  avail_main;
  int                  avail_elan;
  int                  num_chunks ; 
  int                  padding; 
} quadrics_global_vm_pool_t;

typedef struct coll_sync
{
  /* synchronization structures */
  sdramaddr_t          glob_event[QUADRICS_NUM_COLL_ENTRIES]; /* 16*8 */
  sdramaddr_t          nack_event[QUADRICS_NUM_COLL_ENTRIES]; 

  /* subtree control structures */
  E3_Event_Blk        *recv_blk_main[QUADRICS_NUM_COLL_ENTRIES];
  E3_Event_Blk        *nack_blk_main[QUADRICS_NUM_COLL_ENTRIES];

  E3_Event_Blk        *glob_env;            /*env Blk, 64 bytes */

  /* A set of structure for src events */
  E3_Event_Blk        *src_blk_main;
  sdramaddr_t          src_blk_elan;
  sdramaddr_t          src_event; /* 16*8 */
  sdramaddr_t          src_wait_ack;
  sdramaddr_t          src_wait_nack;
  /* 16*8 */
                       
  /*sdramaddr_t          wait_e[QUADRICS_NUM_COLL_ENTRIES];*/
                       
  E3_DMA_MAIN         *sync_dma;      /* root uses it for bcast */
  sdramaddr_t          sync_dma_elan; /* corresponding elan structure */
  sdramaddr_t          chain_dma_elan; /* chained dma for ack */
  sdramaddr_t          chain_dma_elan_nack; /* chained dma for nack */
}
coll_sync_t;

typedef struct bcast_ctrl
{
  /* dma structures for setting bcast */
  E3_DMA_MAIN         *data_dma;   /* used for the first fragment */
  E3_DMA_MAIN         *data_dma0;  /* used for the second fragment */
                       
  /* allow at most one outstanding bcast,
   * It should not be a big problem, since bcast's are serialized anyway.
   * Big size messages can be broken into small ones for better pipelining.
   */ 
  sdramaddr_t          data_dma_elan;
  sdramaddr_t          data_dma0_elan;

  /* sets of notification structures */
  E3_Event_Blk        *src_blk_main; 
  sdramaddr_t          src_blk_elan;
  sdramaddr_t          src_event; 
  sdramaddr_t          wait_event; 
                       
  E3_Event_Blk        *recv_blk_main[QUADRICS_NUM_COLL_ENTRIES]; 
  sdramaddr_t          glob_event[QUADRICS_NUM_COLL_ENTRIES]; /* 16*8 */

  /* An array of control blk structure, for communication,
   * message envelope will precede the data  */
  /*maddr_vm_t           glob_blk[QUADRICS_NUM_COLL_ENTRIES];    */
}
bcast_ctrl_t;

typedef struct segment {
    struct segment *next;            /* link between segments            */
    void           *coll;            /* pointer to the control structure */
    int             bcastVp;         /* The bcast vp                     */
    int             ctx;             /* The context from ELAN_LOCATION   */

    int             min;             /* The min node from ELAN_LOCATION  */
    int             max;             /* The max node from ELAN_LOCATION  */
    int             minVp;           /* The minVp                        */
    int             maxVp;           /* The maxVp                        */

    sdramaddr_t     dma_elan;        /* E3_DMA for data              */
    sdramaddr_t     event;           /* one E3_Event                 */
} bcast_segment_t;

class Broadcaster
{
public:
    int                comm_index;     /* hook to the communicator */
    int                queue_index;    /* seqno for the next index to queue */
    int                ack_index;      /* seqno for the next index to ack */
                                       
    int                desc_avail;     /* Number of available channels    */
    int                total_channels; /* Number of channels    */
    int                faulted;        /* a fault occured before next sync?*/
                                       
/*private:*/                           
    maddr_vm_t         base;           /* The base of the global memory */
    maddr_vm_t         top;            /* The top of the global memory */
                                       
    int                page_size;      /* The page size used by ELAN3 */
                                       
    int                groupSize;      /* The size of the group */
    int                groupRoot;      /* The root of the group */
    int                localRoot;      /* The local master      */
    int                self;           /* The self id in the group */
    int                nhosts;         /* number of hosts in group */
    int                host_index;     /* The host index in group */

    int                master_vp;      /* The manager process */
    int                parent_vp;      /* The parent process */
    int                num_branches;   /* The number of branches */
    int                local_master_vp;   /* The local_master vp */
    int                self_vp;        /* The self vp */
    int                local_size;     /* The number of vps */
    int                branchIndex;    /* The branch Index to the parent*/
    int                branchRatio;    /* the branchRation of the tree */
    bcast_segment_t   *segment;        /* The list of segments  */

    /* Sync and ctrl structure for maximally 16 outstanding bcast */
    coll_sync_t       *sync;           /* The synchronization structures */
    bcast_ctrl_t      *ctrl;           /* The transmission structures */

    quadrics_channel_t ** channels;    /* The list of channels */

    // constructor and destructor
    Broadcaster()      {}
    ~Broadcaster()     {}

    /* 
     * Function to enable the use of hardware based collective 
     * This function is to be called when creating new communicators
     * Also this will be invoked by  lampi_init_postfork_coll_setup 
     * to enable the collective support ULM_COMM_WORLD and ULM_COMM_SELF,
     * which was not done in lampi_init_postfork_resources()
     */

    /* The initianization functions */
    int                init_bcaster(maddr_vm_t, sdramaddr_t);
    void               segment_create();
    void               init_segment_dma(bcast_segment_t *temp);
    int 	       create_syncup_tree();
    int                init_coll_events();
    int                hardware_coll_init();
                       
    /* The messaging functions */
    void               bcast_send (quadrics_channel_t * channel);
    void               bcast_recv (quadrics_channel_t * channel);

    /* The progress engine */
    int                make_progress_bcast();

    /* The synchronization functions */
    void               update_channels(int next_toack);
    int 	       sync_parent();
    int 	       sync_leaves();
};

#endif                             /* End of USE_ELAN_COLL   */

#endif                             /* End of QUADRICS_COLL_H */
