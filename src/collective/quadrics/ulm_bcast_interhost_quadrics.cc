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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <unistd.h>
#include <elan3/elan3.h>
#include <elan/elan.h>
#include <rms/rmscall.h>

#include "client/ULMClient.h"
#include "util/MemFunctions.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "path/quadrics/state.h"
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "include/internal/type_copy.h"
#include "collective/coll_fns.h"

#ifdef USE_ELAN_COLL

#define CHECK_MALLOC(cmd)                       \
do {                                            \
  int ret = 0;                                  \
  ret = (int)(cmd);                             \
  if (!ret)                                     \
    {                                           \
      ulm_err ((#cmd " Malloc failed \n"));     \
      exit (-1);                                \
    }                                           \
} while(0)

/* 
 * ===================================================================== 
 * ================== Initialization functions  ========================
 * ===================================================================== 
 */

/* 
 * A function to initialize a Broadcaster with the provided 
 * global main and elan memory addresses. Referecenced in 
 * lampi_init_postfork_coll_setup()
 */
int Broadcaster::init_bcaster
( maddr_vm_t   glob_main_mem, sdramaddr_t  glob_elan_mem)
{
  int                  i;
  int                  tn = NR_CHANNELS; 

  ELAN3_CTX           *ctx;

  ctx   = quadrics_Glob_Mem_Info->ctx;

  /* First grab the piece of global main memory for broadcast
   * they will then be divided into equal size channles. 
   */
  base  = (maddr_vm_t)((char *)glob_main_mem + BCAST_CTRL_SIZE);
  top   = (maddr_vm_t)((char *)base + COMM_BCAST_MEM_SIZE);

  /* 
     Set up control structures for synchronizing the broacast channels,
   * there will only one per communicator 
   */
  CHECK_MALLOC(sync = (coll_sync_t*) ulm_malloc(sizeof(coll_sync_t) ) );

  /* allocate sync structures */ 
  {
    /* Global main memory, 64 bytes aligned */
    sync->glob_env = (E3_Event_Blk*)glob_main_mem;
    glob_main_mem = (maddr_vm_t) 
      ((char *)glob_main_mem + sizeof(E3_Event_Blk) );

    /* Global elan memory, 64 bytes aligned.
     * Ugly loops to keep the same kind of event sitting together,
     * Assume compiler will not do loop fusion though.
     */
    for ( i = 0 ; i < tn ; i ++ )
    {
      sync->glob_event[i] = glob_elan_mem ;
      glob_elan_mem = (glob_elan_mem + sizeof(E3_BlockCopyEvent) );
    }

    for ( i = 0 ; i < tn ; i ++ )
    {
      sync->nack_event[i] = glob_elan_mem ;
      glob_elan_mem = (glob_elan_mem + sizeof(E3_BlockCopyEvent) );
    }

    /* Allocating synchronization structures from private main/elan memory */
    for ( i = 0 ; i < tn ; i ++ )
    {
      CHECK_MALLOC(sync->recv_blk_main[i] = (E3_Event_Blk*)elan3_allocMain 
	  (ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk))); 
      CHECK_MALLOC(sync->nack_blk_main[i] = (E3_Event_Blk*)elan3_allocMain 
	  (ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk))); 
    }

    CHECK_MALLOC(sync->src_blk_main = (E3_Event_Blk*)elan3_allocMain 
	(ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk))); 
    CHECK_MALLOC(sync->src_blk_elan = elan3_allocElan
       	(ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk)));
    CHECK_MALLOC(sync->src_event    = elan3_allocElan
      (ctx, E3_EVENT_ALIGN, sizeof(E3_BlockCopyEvent)));
    CHECK_MALLOC(sync->src_wait_ack = 
      elan3_allocElan(ctx, E3_EVENT_ALIGN, tn * sizeof(E3_WaitEvent0)));
    CHECK_MALLOC(sync->src_wait_nack = 
      elan3_allocElan(ctx, E3_EVENT_ALIGN, tn * sizeof(E3_WaitEvent0)));

    CHECK_MALLOC(sync->sync_dma      = (E3_DMA_MAIN *)elan3_allocMain 
      (ctx, E3_DMA_ALIGN, sizeof(E3_DMA_MAIN))); 
    CHECK_MALLOC(sync->sync_dma_elan = elan3_allocElan
      (ctx, E3_DMA_ALIGN, sizeof(E3_DMA_MAIN))); 
    CHECK_MALLOC(sync->chain_dma_elan = elan3_allocElan
      (ctx, E3_DMA_ALIGN, tn*sizeof(E3_DMA_MAIN))); 
    CHECK_MALLOC(sync->chain_dma_elan_nack = elan3_allocElan
      (ctx, E3_DMA_ALIGN, tn*sizeof(E3_DMA_MAIN))); 
  }

  /* Set up control structures for the bcast channels */
  CHECK_MALLOC(channels = (quadrics_channel_t **) 
      ulm_malloc(sizeof(quadrics_channel_t*) * tn));
  CHECK_MALLOC(ctrl = (bcast_ctrl_t*) ulm_malloc(sizeof(bcast_ctrl_t))); 

  for ( i = 0 ; i < tn ; i ++ ) 
  {
    CHECK_MALLOC (channels[i] = (quadrics_channel_t *)
      ulm_malloc(sizeof(quadrics_channel_t) ) );
  }

  /* allocate transmission control structures */
  {
    /* Structures that needs to be sitting in global memory */
    for ( i = 0 ; i < tn ; i ++ )
    {
      ctrl->glob_event[i] = glob_elan_mem;
      glob_elan_mem = (glob_elan_mem + sizeof(E3_BlockCopyEvent) );
    }

    /* Structures that suffice to be allocated from local memory */
    for ( i = 0 ; i < tn ; i ++ )
      CHECK_MALLOC(ctrl->recv_blk_main[i] = (E3_Event_Blk*)elan3_allocMain 
      (ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk))); 

    CHECK_MALLOC(ctrl->src_blk_main = (E3_Event_Blk*)elan3_allocMain 
      (ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk))); 
    CHECK_MALLOC(ctrl->src_blk_elan = elan3_allocElan
      (ctx, E3_BLK_ALIGN, sizeof(E3_Event_Blk)));
  }

  /* Devide the global bcast buffer into channels */
  for ( i = 0 ; i < tn ; i++)
  {
    channels[i]->mcast_buff    = (char*)base + (BCAST_CHAN_LENGTH)*i;
    channels[i]->index         = i;
  }
  return ULM_SUCCESS;
}

/* A function to reset all the collective control structures.
 * Used to recycle the broadcasters, fault recovery from message
 * problems (retransmissions).
 */
int Broadcaster::reset_coll_events()
{
  int                    i;
  int                    tn = total_channels;

  ELAN3_CTX             *ctx;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;

  {
    E3_Event_Blk      * blk ;
    sdramaddr_t         blk_elan ;
    sdramaddr_t         event;
    sdramaddr_t         waitevent;
    E3_DMA_MAIN       * sync_dma;

    /* 
     * Zero-out sync_dma to reset the synchronization structures 
     */
    sync_dma = sync->sync_dma;
    bzero(sync_dma, sizeof(E3_DMA_MAIN));
    elan3_copy32_to_sdram(ctx->sdram, sync_dma, sync->sync_dma_elan);

    for ( i = 0 ; i < tn; i ++ )
    {
      /* 
       * For receive events related to the synchronization,
       * Including both ACK-related and NACK-related ones
       */
      blk        = sync->recv_blk_main[i];
      blk_elan   = sync->src_blk_elan;
      event      = sync->glob_event[i];
      waitevent  = sync->src_wait_ack + sizeof(E3_WaitEvent0)*i;

      elan3_initevent(ctx, event);
      elan3_primeevent(ctx, event, 0);
      elan3_initevent(ctx, waitevent);
      elan3_primeevent(ctx, waitevent, 0);
      E3_RESET_BCOPY_BLOCK(blk);
      elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
	  sync->chain_dma_elan + i*sizeof(E3_DMA_MAIN));

      blk        = sync->nack_blk_main[i];
      blk_elan   = sync->src_blk_elan;
      event      = sync->nack_event[i];
      waitevent  = sync->src_wait_nack + sizeof(E3_WaitEvent0)*i;

      elan3_initevent(ctx, event);
      elan3_primeevent(ctx, event, 0);
      elan3_initevent(ctx, waitevent);
      elan3_primeevent(ctx, waitevent, 0);
      E3_RESET_BCOPY_BLOCK(blk);
      elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
	  sync->chain_dma_elan + i*sizeof(E3_DMA_MAIN));

      /* 
       * For local events to the sender.  We still use the same name 
       * 'recv_blk_main', but it actually refer to the BlockCopyEvent 
       * triggered locally when send completes.
       */
      blk        = ctrl->recv_blk_main[i];
      blk_elan   = ctrl->src_blk_elan;
      event      = ctrl->glob_event[i];

      elan3_initevent(ctx, event);
      E3_RESET_BCOPY_BLOCK(blk);
      elan3_primeevent(ctx, event, 0);
    }
  }

  /* Always reset the descriptor index about the last completed bcast 
   * operation */
  ((ulm_coll_env_t*) sync->glob_env)->desc_index = 0;

  /* A barrier is needed after this. */
  END_MARK;
  return ULM_SUCCESS;
}

/* A function does initialize all structures for broadcast, this one
 * is intendend to be used when the broadcaster is initialized
 */
int Broadcaster::init_coll_events
(int start_index, int end_index, int set_sync_dma)
{
  int                    i,j;
  int                    tn = total_channels;
  ELAN3_CTX             *ctx;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;

  {
    /* Prime the recv events for asynchronous recv */
    E3_Event_Blk      * blk ;
    sdramaddr_t         blk_elan ;
    sdramaddr_t         event;
    sdramaddr_t         waitevent;
    E3_DMA_MAIN       * sync_dma;

    /* 
     * For receive events related to the synchronization,
     * Including both ACK-related and NACK-related ones
     */
    sync_dma = sync->sync_dma;
    sync_dma->dma_type = 
      E3_DMA_TYPE (quadrics_Glob_Mem_Info->dmaType, DMA_WRITE, 
	  DMA_NORMAL, quadrics_Glob_Mem_Info->retryCount);

    sync_dma->dma_dest      = 0;
    sync_dma->dma_source    = 0;
    sync_dma->dma_destEvent = 0; 
    sync_dma->dma_srcEvent  = 0; 

    if (set_sync_dma)
    {
      int              remote_vp;

      /* To which vp should I trigger the event */
      remote_vp = num_branches ? local_master_vp : parent_vp;
      sync_dma->dma_srcCookieVProc=  
	elan3_local_cookie(ctx, self_vp, remote_vp);
      sync_dma->dma_destCookieVProc= remote_vp;
      elan3_copy32_to_sdram(ctx->sdram, sync_dma, sync->sync_dma_elan);
    }

    for ( j = start_index ; j <= end_index ; j ++ )
    {
      i = j % tn;

      /* Initialize the synchronization structures */
      blk        = sync->recv_blk_main[i];
      blk_elan   = sync->src_blk_elan;
      event      = sync->glob_event[i];
      waitevent  = sync->src_wait_ack + sizeof(E3_WaitEvent0)*i;

      /* Every Body update its parent with the chained dma. */
      sync_dma->dma_srcCookieVProc=  
	elan3_local_cookie(ctx, self_vp, parent_vp);
      sync_dma->dma_destCookieVProc= parent_vp;

      if ( self_vp == master_vp )
      {
	elan3_initevent_main(ctx, event, blk_elan, blk);
	E3_RESET_BCOPY_BLOCK(blk);
	elan3_primeevent(ctx, event, num_branches + 1);
      }
      else if ( self_vp == local_master_vp )
      {
	elan3_initevent(ctx, event);
	elan3_waitdmaevent0(ctx, 
	    (sync->chain_dma_elan + i * sizeof(E3_DMA_MAIN)),
	    event, waitevent, num_branches + 1); 
	sync_dma->dma_destEvent = S2E(event); 
	sync_dma->dma_srcEvent  = 0;
	elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
	    sync->chain_dma_elan + i*sizeof(E3_DMA_MAIN));
      }

      blk        = sync->nack_blk_main[i];
      blk_elan   = sync->src_blk_elan;
      event      = sync->nack_event[i];
      waitevent  = sync->src_wait_nack + sizeof(E3_WaitEvent0)*i;

      /* NACK is generated to the master_vp directly */
      sync_dma->dma_srcCookieVProc=  
	elan3_local_cookie(ctx, self_vp, master_vp);
      sync_dma->dma_destCookieVProc= master_vp;

      if ( self_vp == master_vp )
      {
	/* One nack wins over all acks */
	elan3_initevent_main(ctx, event, blk_elan, blk);
	E3_RESET_BCOPY_BLOCK(blk);
	elan3_primeevent(ctx, event, 1);
      }
      else if ( self_vp == local_master_vp )
      {
	sync_dma->dma_destEvent = S2E(event);
	sync_dma->dma_srcEvent  = 0;
	elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
	    sync->chain_dma_elan_nack + i*sizeof(E3_DMA_MAIN));
      }

      /* Ctrl Ones */
      blk        = ctrl->recv_blk_main[i];
      blk_elan   = ctrl->src_blk_elan;
      event      = ctrl->glob_event[i];

      elan3_initevent_main (ctx, event, blk_elan, blk);
      E3_RESET_BCOPY_BLOCK(blk);
      elan3_primeevent(ctx, event, 1);
    }
  }
  ((ulm_coll_env_t*) sync->glob_env)->desc_index = 0;

  /* A barrier is needed hereafter if this is invoked due to a nack. */
  END_MARK;
  return ULM_SUCCESS;
}

/* 
 * This function is used to create DMA structures for each segment
 * Since we use a linear list of segments, segment other than the last
 * has to trigger the start of its next neighbours.
 * Segmemnts other than the first has to be waited on the event 
 * from its previous neighbour.
 */
void 
Broadcaster::init_segment_dma(bcast_segment_t *temp)
{
  E3_DMA_MAIN            temp_dma;
  E3_DMA_MAIN           *sync_dma;
  ELAN3_CTX             *ctx;
  int                    slot_offset ;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;

  /* data dma for broadcasting */
  elan3_initevent_main (ctx, sync->src_event,
      sync->src_blk_elan, sync->src_blk_main);
  E3_RESET_BCOPY_BLOCK(sync->src_blk_main);
  elan3_primeevent(ctx, sync->src_event, 1);

  sync_dma = (E3_DMA_MAIN*)&temp_dma;
  sync_dma->dma_type = 
    E3_DMA_TYPE (quadrics_Glob_Mem_Info->dmaType, DMA_WRITE, 
	DMA_NORMAL_BROADCAST, quadrics_Glob_Mem_Info->retryCount);

  sync_dma->dma_srcCookieVProc =  
    elan3_local_cookie(ctx, self_vp, temp->bcastVp);
  sync_dma->dma_destCookieVProc = temp->bcastVp;

  /* The following four and the events need to be updated per bcast */
  sync_dma->dma_dest      = 0;  /* to be updated */
  sync_dma->dma_source    = 0;  /* to be updated */
  sync_dma->dma_size      = 0;  /* to be updated */
  sync_dma->dma_destEvent = 0;  /* to be updated */

  /* For different channels, they use the same set of segment for bcast.
   * Here for a particular segment, 
   * initialize the transmission DMAs for all the Channels.
   */
  for ( int k = 0 ; k < total_channels ; k ++)
  {
    for ( int fragment = 0 ; fragment < MAX_BCAST_FRAGS ; fragment ++)  
    {
      slot_offset = (k * MAX_BCAST_FRAGS + fragment);
      if ( fragment == 0)
      {
	sync_dma->dma_srcEvent  = S2E(ctrl->glob_event[k]);
	sync_dma->dma_destEvent = S2E(ctrl->glob_event[k]);
	sync_dma->dma_source    = M2E(channels[k]->mcast_buff);
	sync_dma->dma_dest      = M2E(channels[k]->mcast_buff);
      }
      else
      {
	sync_dma->dma_srcEvent  = 0; 
	sync_dma->dma_destEvent = 0;
	sync_dma->dma_source    = 0;
	sync_dma->dma_dest      = 0;
      }
      if (temp->next)
      {
	sync_dma->dma_srcEvent  = 
	  S2E((temp->event + slot_offset * sizeof(E3_Event)));
	elan3_initevent(ctx, 
	    (temp->event + slot_offset * sizeof(E3_Event)));
	elan3_waitdmaevent(ctx, (temp->next->dma_elan + 
	      slot_offset *sizeof(E3_DMA_MAIN)), 
	    (temp->event + slot_offset * sizeof (E3_Event))); 
      }
      elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
	(temp->dma_elan + slot_offset * sizeof(E3_DMA)));
    }
  }

  /* Initialize the synchronization DMA */
  slot_offset = (total_channels * MAX_BCAST_FRAGS );
  sync_dma->dma_dest   = M2E(sync->glob_env);
  sync_dma->dma_source = M2E(sync->glob_env);
  sync_dma->dma_size   = 64;     
  sync_dma->dma_destEvent = 0;  
  if (temp->next)
  {
    sync_dma->dma_srcEvent  = 
      S2E((temp->event + slot_offset * sizeof(E3_Event)));
    elan3_initevent(ctx,  
	(temp->event + slot_offset * sizeof(E3_Event)));
    elan3_waitdmaevent(ctx, 
	(temp->next->dma_elan + slot_offset *sizeof(E3_DMA_MAIN)), 
	(temp->event + slot_offset * sizeof (E3_Event))); 
  }
  else
  {
    sync_dma->dma_srcEvent  = S2E(sync->src_event);
  }

  elan3_copy32_to_sdram(ctx->sdram, sync_dma, 
    (temp->dma_elan + slot_offset * sizeof(E3_DMA)));

  END_MARK;
}

/* 
 * This is following the challenges made in src/init/init_rms.cc,
 * to accommodate the QCS update. May not cover all different quadrics 
 * releases across different platforms.
 */
#if QSNETLIBS_VERSION_CODE >= QSNETLIBS_VERSION(1,4,14)
#define elan3_nvps elan_nvps 
#define elan3_vp2location elan_vp2location
#define elan3_location2vp elan_location2vp
#define elan3_maxlocal elan_maxlocal
#define elan3_nlocal elan_nlocal

/* The following two are bad, but I ensure Node and Context are not
 * used for other purpose in this file. It shoud be OK.
 */
#define Node loc_node
#define Context loc_context
#endif

/* This function setup the segments for the broadcast operation.
 * For the time being, we use only a linear list of segments.
 * Later optimization will be done to use a tree. 
 */
void Broadcaster::segment_create(void)
{
  int            vp;
  int            nvp;
  int            member;
  ELAN_LOCATION  location;
  ELAN_LOCATION  mylocation;
  int            max_node = 0;
  int            max_ctx  = 0;
  int            min_node = 1<<20;
  int            min_ctx  = 1<<20;
  int            node_extent = 0;
  int            first_ctx = 0;
  int            llocalId;
  int            node_num;
  int            nself = 0;
  int            parent_host;
  bcast_segment_t *first;
  int            i;
  int           *nid_to_gid_tab;
  int           *vp_to_gid_tab;
  Group          *localGroup;

  ELAN3_CTX             *ctx;
  ELAN_CAPABILITY       *cap;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;
  cap         = quadrics_Glob_Mem_Info->cap;

  localGroup = communicators[comm_index]->localGroup;
  nvp = elan3_nvps(cap);
  self_vp = elan3_process(ctx);

  first = segment = NULL;

  /* Find the bounds of the "box" that thejob runs in */
  for ( member = 0 ; member < groupSize; member++ )
  {
      vp = localGroup->mapGroupProcIDToGlobalProcID[member];
      /* Assume elan vp is equivalent to global process id */
      if ( vp < 0 || vp >= nvp)
        ulm_err(("Invalid Vp Id\n")); 
      
      location = elan3_vp2location(vp,cap);
      
      if ( location.Node > max_node )
          max_node = location.Node;
      if ( location.Node < min_node )
          min_node = location.Node;
      if ( location.Context > max_ctx )
          max_ctx = location.Context;
      if ( location.Context < min_ctx )
          min_ctx = location.Context;
  }

  node_extent = max_node - min_node + 1;

  /* Will be set to ELAN_INVALID_PROCESS for non group members */
  nid_to_gid_tab = (int*)malloc(sizeof(int)*node_extent);
  vp_to_gid_tab  = (int*)malloc(sizeof(int)*nvp);

  if ( nid_to_gid_tab == NULL  || !vp_to_gid_tab )
  {
    ulm_exit((-1, "Failed to allocate memory\n"));
  }
  
  for ( i = 0 ; i < node_extent ; i++ )
  {
    nid_to_gid_tab[i] = ELAN_INVALID_PROCESS;
  }

  for ( i = 0 ; i < nvp; i++)
  {
    vp_to_gid_tab[i] = ELAN_INVALID_PROCESS;
  }

  for ( i = 0 ; i < groupSize; i++)
  {
    member = localGroup->mapGroupProcIDToGlobalProcID[i];
    vp_to_gid_tab[member] = i;
  }

  /* Walk throught the grid of possible members, trying to find segments */
  mylocation     = elan3_vp2location(self_vp, cap);

  node_num = 0;
  for ( location.Node = 0; location.Node <= max_node ; location.Node++ )
  {  
      llocalId = -1;
      /* For each context */
      for ( location.Context = 0; location.Context <= max_ctx ; 
          location.Context++ )
      {
          vp = elan3_location2vp(location,cap);
          
          /* It's not a valid vp or it's not in our group */
          if ( vp == -1 || vp == ELAN_INVALID_PROCESS ||
	      vp_to_gid_tab[vp] == ELAN_INVALID_PROCESS)
	    continue;
          
          llocalId++;
          
          if ( llocalId == 0 )
          {
	    first_ctx = location.Context;
	    if ( node_num == 0)
	      groupRoot = vp_to_gid_tab[vp];

	    if ( mylocation.Node == location.Node )
	      localRoot =  vp_to_gid_tab[vp];
          }
      }

      /* There are no group members on this node */
      if ( llocalId == -1 )
          continue;

      /* If this is my node then stash nLocal and nself*/
      if ( mylocation.Node == location.Node )
      {
          nself = node_num;
          assert ( nself == host_index);
          host_index = node_num;
      }
      
      if ( mylocation.Node == location.Node )
      {
        location.Context = first_ctx;
        vp = elan3_location2vp(location,cap);
      }
      else 
      {
          /* There is no segment list, this must be the first node */
          if ( ! first )
          {
	    CHECK_MALLOC(first = segment = (bcast_segment_t*)
	      ulm_malloc(sizeof(bcast_segment_t)));
	    memset(segment, 0, sizeof(bcast_segment_t));
	    segment->min    = location.Node;
	    segment->max    = location.Node;
	    segment->ctx    = first_ctx;
	    location.Context = segment->ctx;
	    vp = elan3_location2vp(location,cap);
          } 
          else 
          {
	    /* It is wrong to take the previous ctx, 
	     * Following libelan will produce a very subtle bug
	     */
	    /*location.Context = segment->ctx;*/
	    location.Context = first_ctx;
	    vp = elan3_location2vp(location,cap);
	    
	    /* Does this node extend the current context ? */
	    if ( location.Node == segment->max + 1 &&
		 location.Context == segment->ctx &&
		 vp != -1 &&
		 vp != ELAN_INVALID_PROCESS &&
		 vp_to_gid_tab[vp] != ELAN_INVALID_PROCESS )
	    {
	      segment->max++;
	    } 
	    else 
	    {
		/* Create a new segment */
		CHECK_MALLOC(segment->next  = (bcast_segment_t*)
		  ulm_malloc(sizeof(bcast_segment_t)));
		segment        = segment->next;
		memset(segment, 0, sizeof(bcast_segment_t));
		segment->min   = location.Node;
		segment->max   = location.Node;
		segment->ctx   = first_ctx;

		/* Work out the vp based on the new context */
		location.Context = segment->ctx;
		vp = elan3_location2vp(location,cap);
	    }
          }
      }

    if ( nself == node_num ) 
    {
      local_master_vp = vp;
      local_size      = llocalId + 1;
    }

    /* Store a link between nodeId and netVp */
    nid_to_gid_tab[node_num] = vp;

    /* Increase the number of nodes */  
    node_num++;
  }

  nhosts = node_num;

  if ( comm_index == ULM_COMM_WORLD )
    quadrics_Glob_Mem_Info->nhosts = node_num;

  segment = first;
  while (segment)
  {
    location.Context = segment->ctx;
    location.Node = segment->min;
    if ( segment->min == segment->max)
    {
      segment->minVp = segment->maxVp = 
      segment->bcastVp = elan3_location2vp(location, cap); 
    }
    else 
    {
      segment->minVp = elan3_location2vp(location, cap); 
      location.Node  = segment->max;
      segment->maxVp = elan3_location2vp(location, cap); 
      segment->bcastVp  = elan3_allocatebcastvp(ctx);
      if ((elan3_addbcastvp(ctx, segment->bcastVp, 
      	segment->minVp, segment->maxVp) == -1))
      {
        ulm_exit((-1, "Not able to add Bcast VP\n"));
      }
    }

    CHECK_MALLOC(segment->dma_elan = 
      elan3_allocElan(ctx, E3_DMA_ALIGN, 
        (total_channels * MAX_BCAST_FRAGS + 1)*sizeof(E3_DMA)));

    // using E3_EVENT_ALIGN (8) seems to cause problems on QSC
    // If the elan3_allocElan() is called, then (many elan calls later)
    // the quadrics NIC will gt confused and issue a SEGV.   
    // useing the more resctrive DMA_ALIGN (64) seems to fix the problem
    CHECK_MALLOC(segment->event =  
         elan3_allocElan(ctx, /*E3_EVENT_ALIGN*/ E3_DMA_ALIGN, 
        (total_channels * MAX_BCAST_FRAGS + 1)*sizeof(E3_Event)));
    segment = segment->next;
  }

  segment = first;

  /* Find the parent host index and its vp */
  parent_host = create_syncup_tree();

  master_vp = nid_to_gid_tab[0];
  parent_vp = nid_to_gid_tab[parent_host];

  ulm_free(nid_to_gid_tab);
  ulm_free(vp_to_gid_tab);

  END_MARK;
}

/* To set up a balance tree for synchronization */
int Broadcaster::create_syncup_tree()
{
  int start_node, total_nodes, parent_index, self_index;

  START_MARK;

  /* Find my location in a balanced tree rooted at process 0, where each
   * process in the tree has at most 'branchRatio' immediate
   * descendents.
   * Process 'start_node' is the root of a sub-tree of size 'total_nodes'. She
   * therefore has 'total_nodes' - 1 descendents that she divides as equally as 
   * possible among her branches.
   *
   * Note also that the group index number of the processes in any
   * sub-tree are contiguous (important for parallel prefix).  
   */

  start_node = 0;                  
  total_nodes = nhosts;            
  self_index = host_index;
  branchIndex = -1;               
  parent_index = -1;               

  while (start_node != self_index) 
  {                                
      int extra, cutoff, distance, non_extra_index;

      parent_index = start_node;
      start_node++;                  
      total_nodes--;                

      extra = total_nodes % branchRatio;
      total_nodes = total_nodes / branchRatio + 1;
      cutoff = extra * total_nodes;
      distance = self_index - start_node;

      if (distance < cutoff)                      
      {                              
	  branchIndex = distance / total_nodes;
	  start_node += branchIndex * total_nodes;
      }
      else                            
      {
	  total_nodes--;
	  non_extra_index = (distance - cutoff) / total_nodes; 
	  branchIndex = extra + non_extra_index;    
	  start_node += cutoff + non_extra_index * total_nodes;  
      }
  }
  start_node++;                    
  total_nodes --;                 

  /* leaves + their parents may have # children < branchRatio */
  num_branches = (total_nodes < branchRatio) ? total_nodes : branchRatio;

  if (host_index == 0) parent_index = 0;

  END_MARK;
  return parent_index;
}

/* The function to free the collective */
int Broadcaster::broadcaster_free()
{
  bcast_segment_t *first, *temp;
  int rc;
  START_MARK;

  ELAN3_CTX             *ctx;

  ctx = quadrics_Glob_Mem_Info->ctx;

  /* Assert no more traffic */
  while (ack_index < queue_index && rc != ULM_ERR_BCAST_FAIL)
    rc = make_progress_bcast();

  reset_coll_events();
  /* free  the segements, during which bcast_vp is to be freed*/
  first = segment;
  while(first)
  {
    temp = first;
    first = first->next;

    /* Free the elan structures */
    elan3_freeElan(ctx, temp->event);
    elan3_freeElan(ctx, temp->dma_elan);

    /* Free the segment */
    ulm_free(temp);
  }
 
  /* Reset the fields, do not take the risk of resetting 
   * synchronization structures 
   */
  bzero((void*)&comm_index, ((int) &segment - (int) &comm_index )); 

  /* Make sure the first segment is reset */
  segment= 0;

  END_MARK;
  return rc;
}

/* Hardware based collectives initialization function. */
int Broadcaster::hardware_coll_init(void)
{
    Group *localGroup; 
    bcast_segment_t *first;
    int nseg;

    START_MARK;

    /* leave the following as it is for now */
    localGroup  = communicators[comm_index]->localGroup;
    queue_index = ack_index = 0;
    page_size   = sysconf(_SC_PAGESIZE);

    groupSize   = localGroup->groupSize;
    nhosts      = localGroup->numberOfHostsInGroup;

    groupRoot   = localGroup->groupHostData[0].groupProcIDOnHost[0];
    host_index  = localGroup->hostIndexInGroup;
    localRoot   = localGroup->groupHostData[host_index].groupProcIDOnHost[0];
    self        = localGroup->ProcID;
    branchRatio = SYNC_BRANCH_RATIO ;
    total_channels  = NR_CHANNELS; 
    faulted     = 0 ;        

    branchRatio = getenv("LAMPI_BCAST_BRANCHRATIO") ? 
      atoi (getenv("LAMPI_BCAST_BRANCHRATIO") ) :
      SYNC_BRANCH_RATIO ;
    total_channels  = getenv("LAMPI_BCAST_CHANNELS") ? 
      atoi (getenv("LAMPI_BCAST_CHANNELS") ) :
      NR_CHANNELS; 

    if ( !(branchRatio >= 2 && total_channels >= 2) )
    {
      ulm_exit((-1, "Please reset LAMPI_BCAST_BRANCHRATIO " 
	  "or LAMPI_BCAST_CHANNELS\n" ));
    }

    desc_avail = total_channels;

    /* Create the segment for noncontiguous vps */
    segment_create();

    /* count number of segments: */
    nseg=0;
    first = segment;
    while(first)
    {
      first = first->next;
      ++nseg;
    }
    if (nseg>1)
        if (localGroup->ProcID==0)  
            ulm_err(("comm=%i number of segments: %i\n",comm_index,nseg ));

    if (nseg>8) {
        if (localGroup->ProcID==0)  
            ulm_err(("Warning: comm=%i process distribution non-contigious (%i segments). Disabling hardware bcast\n",
                     comm_index,nseg ));
        return ULM_ERR_BCAST_INIT;
    }


    /* Make sure the number of host is greater than 1 */
    if ( nhosts <=1 )
      return ULM_ERR_BCAST_INIT;

    init_coll_events(0, total_channels-1, 1);

    first = segment;
    while(first)
    {
      init_segment_dma(first);
      first = first->next;
    }

    /* A barrier is needed hereafter to synchronize the processes */
    END_MARK;
    return ULM_SUCCESS;
}

/* 
 * ===================================================================== 
 * ================== Transmission functions  ==========================
 * ===================================================================== 
 */

/* 
 * A function to pack 'count' 'dtype' data. 
 * For each dtype, pack all the data from the first pair to the last 
 */
enum {
  COPYING          = 0,
  PACKING          = 1,
  UNPACKING        = 2,
};

/* Since the basic packing and unpacking function is not really
 * versatile in terms of functionalities, I have to go through
 * all the pains to create these functions. umh.., not fun!.
 */
inline unsigned int pack_buffer(void *src_addr, void * dest_addr, 
    int count, ULMType_t * dtype, 
    int offset, int length,
    int packing, int compute_crc)
{
  unsigned char * from ; 
  unsigned char * to   ;

  /* contiguous data */
  if (packing == COPYING) {
    from = (unsigned char*)src_addr + offset;
    to   = (unsigned char*)dest_addr+ offset;
    if (compute_crc) {
	  if (usecrc()) {
	      return bcopy_uicrc((from), to, length, length);
	  }
	  else {
	      return bcopy_uicsum((from), to, length, length);
	  }
      }
      else 
      {
	  MEMCOPY_FUNC((from), to, length);
	  return 0;
      }
  }
  else
  {
    /* non-contiguous data */
    int tm_init = 0;
    int data_remaining ;
    int init_cnt ; 
    int ti;
    int offset_in_map ;
    int len_to_copy;

    ULMTypeMapElt_t *tmap;
    int dtype_cnt;
    unsigned char *start_addr;
    unsigned char *temp_addr;
    int len_copied;
    unsigned int csum = 0, ui1 = 0, ui2 = 0;

    tmap = dtype->type_map;
    data_remaining = (int)(offset % dtype->packed_size);
    init_cnt = offset / dtype->packed_size;

    for (ti = 0; ti < dtype->num_pairs; ti++) {
	if (tmap[ti].seq_offset == data_remaining) {
	    tm_init = ti;
	    break;
	} else if (tmap[ti].seq_offset > data_remaining) {
	    tm_init = ti - 1;
	    break;
	}
    }

    offset_in_map = offset - init_cnt * dtype->packed_size 
	- tmap[tm_init].seq_offset ;
    len_to_copy = tmap[tm_init].size - offset_in_map;
    len_to_copy = (len_to_copy > length ) ? length: len_to_copy;

    // handle first typemap pair
    if ( packing == PACKING )
    {
      start_addr = (unsigned char *)src_addr + init_cnt * dtype->extent;
      from = (unsigned char *)start_addr
	+ tmap[tm_init].offset + offset_in_map;
      temp_addr = 
      to   = (unsigned char *)dest_addr + offset;
    }
    else
    {
      start_addr = (unsigned char *)dest_addr + init_cnt * dtype->extent;
      temp_addr = from = (unsigned char *)src_addr + offset;
      to   = (unsigned char *)start_addr
       	+ tmap[tm_init].offset + offset_in_map;
    }

    if (compute_crc) {
        if (usecrc()) {
            csum = bcopy_uicrc(from, to, len_to_copy, len_to_copy, CRC_INITIAL_REGISTER);
        }
        else {
            csum = bcopy_uicsum(from, to, len_to_copy, len_to_copy, &ui1, &ui2);
        }
    }
    else {
        MEMCOPY_FUNC(from, to, len_to_copy);
    }

    len_copied = len_to_copy;

    tm_init++;

    for (dtype_cnt = init_cnt; dtype_cnt < count; dtype_cnt++) 
    {
      for (ti = tm_init; ti < dtype->num_pairs; ti++) 
      {
       	if  ( packing == PACKING )
       	{
	  from = (unsigned char *)start_addr + tmap[ti].offset;
	  to   = (unsigned char *)temp_addr + len_copied;
       	}
       	else
       	{
	  from = (unsigned char *)temp_addr + len_copied;
	  to   = (unsigned char *)start_addr + tmap[ti].offset;
       	}

	/* determine the length */
       	len_to_copy = (length - len_copied >= (int)tmap[ti].size) ?
	  tmap[ti].size : (length - len_copied);

	if (len_to_copy == 0) 
	  return csum;

	if (compute_crc) 
	{
	  if (usecrc()) 
	  {
	    csum = bcopy_uicrc(from, to, len_to_copy, len_to_copy, csum);
	  }
	  else 
	  { 
	    csum += bcopy_uicsum(from, to, len_to_copy, 
		len_to_copy, &ui1, &ui2);
	  }
       	}
       	else 
	{ 
	  MEMCOPY_FUNC(from, to, len_to_copy);
       	}
       	len_copied += len_to_copy;
      }
      tm_init = ti= 0;
      start_addr +=  dtype->extent;
    }
    return csum;
  }
}

/* 
 * This function is introduced to facilitate shared memory implementation.
 */
static void
bcast_pack_for_dma(maddr_vm_t send_addr, maddr_vm_t mcast_buff, int count, 
    ULMType_t * dtype, int packing, int copying, int elan_bugged, 
    int DoChecksum, int dma_headers)
{
    /* Added new flag: dma_headers == 0
     * In this case, we are using this routine to simply copy the
     * data into the mcast_buff so that it can be unpacked on the 
     * same node with bcat_unpack_after_dma() 
     * note: dma_headers==0 assumes DoChecksum==0
     */
  quadrics_coll_header_t        * mesg_env;
  int size ;
  unsigned char * start_addr ; 

  START_MARK;
  size = count * dtype->packed_size;
  mesg_env = (quadrics_coll_header_t*) mcast_buff;

  if ( dtype !=NULL && dtype->layout != CONTIGUOUS && dtype->num_pairs !=0)
  {
    start_addr= (unsigned char*)send_addr - dtype->type_map[0].offset; 
  }
  else
  {
    start_addr = (unsigned char*)send_addr;
  }

  if ( size <= BCAST_INLINE_SIZE)
  {
    /* contiguous data in non-elan addressable memory or 
     * non-contiguous data - checksum almost free */
    pack_buffer(start_addr, mesg_env->sform.inline_data, 
	count, dtype, 0, size, packing, 0 );

    /* Mark the mesg type */
#if 1
    if (dma_headers) {
        mesg_env->sform.coll_type  = (size <= BCAST_INLINE_SIZE_SMALL)? 
            BCAST_MESG_TYPE_INLINE_S: BCAST_MESG_TYPE_INLINE_L;
        mesg_env->sform.mesg_length   = size;
        mesg_env->sform.data_length   = 
            (size <= BCAST_INLINE_SIZE_SMALL)? 64 : 128;
    }

    if ( DoChecksum)
    {
      if ( size <= BCAST_INLINE_SIZE_SMALL )
      {
	mesg_env->sform.checksum      = usecrc()?
	  uicrc  (mesg_env, 64): uicsum (mesg_env, 64);
      }
      else
      {
	mesg_env->lform.checksum      = usecrc()?
	  uicrc  (mesg_env, 128): uicsum (mesg_env, 128);
      }
    }
#else
    if (dma_headers) {
        mesg_env->sform.coll_type  = BCAST_MESG_TYPE_INLINE_L;
        mesg_env->sform.mesg_length   = size;
        mesg_env->sform.data_length   = 128;
    }

    if ( DoChecksum)
    {
      {
	mesg_env->lform.checksum      = usecrc()?
	  uicrc  (mesg_env, 128): uicsum (mesg_env, 128);
      }
    }
#endif
    return ;
  }

  /* Copy data over to the mcast_buff */
  if ( copying || packing ) 
  {
    /* Mark the mesg type */
    if (dma_headers) {
        mesg_env->sform.coll_type     = BCAST_MESG_TYPE_SHORT;
        mesg_env->sform.mesg_length   = size;
        mesg_env->sform.data_length   = 64 + ELAN_ALIGNUP(size, 64);
        mesg_env->sform.data_checksum = 
            pack_buffer(start_addr, mesg_env->sform.data, 
                        count, dtype, 0, size, packing, DoChecksum);
        mesg_env->sform.checksum      = usecrc()?
            uicrc  (mesg_env, 64): uicsum (mesg_env, 64);
    }else{
        pack_buffer(start_addr, mesg_env->sform.data, 
                    count, dtype, 0, size, packing, DoChecksum);
    }

    return ;
  }
  else 
  {
    if (elan_bugged)
    {
      /* Generate CRC for the second piece */
      int fragged_len = (64 - ((unsigned int) send_addr & 0x3F));

      /* Copy the first frag, generate checksum for the first piece*/
      bcopy(send_addr, mesg_env->lform.inline_data, fragged_len);

      /* Mark the mesg type */
      if (dma_headers) {
      mesg_env->lform.coll_type     = BCAST_MESG_TYPE_BUGGY;
      mesg_env->lform.mesg_length   = size;
      mesg_env->lform.frag_length   = fragged_len;
      mesg_env->lform.data_length   = 128 
	+ ELAN_ALIGNUP((size - fragged_len), E3_BLK_ALIGN );
      }

      if (DoChecksum)
	{
	  mesg_env->lform.data_checksum = usecrc()?
	    uicrc (((char*)send_addr + fragged_len), size-fragged_len):
	    uicsum(((char*)send_addr + fragged_len), size-fragged_len);
	  mesg_env->lform.checksum      = usecrc()?
	    uicrc  (mesg_env, 128): uicsum (mesg_env, 128);
	}
    }
    else
    {
      /* Mark the mesg type */
      if (dma_headers) {
      mesg_env->sform.coll_type     = BCAST_MESG_TYPE_LONG;
      mesg_env->sform.mesg_length   = size;
      mesg_env->sform.frag_length   = 0;
      mesg_env->sform.data_length   = 64 + ELAN_ALIGNUP(size, 64);
      }

      if (DoChecksum)
	{
	  mesg_env->sform.data_checksum = usecrc()?
	    uicrc (send_addr, size): uicsum (send_addr, size);
	  mesg_env->sform.checksum      = usecrc()?
	    uicrc  (mesg_env, 64): uicsum (mesg_env, 64);
	}
    }
  }
  END_MARK;
}

/* This function is introduced to for shared memory implementation.
 * Only the process involved in off-host communication should do this. 
 * But the real performance needs to check 
 */
static int 
bcast_unpack_after_dma(maddr_vm_t mcast_buff, maddr_vm_t recv_addr,
	  int count, ULMType_t * dtype, int size, int DoChecksum)
{
  quadrics_coll_header_t * header;
  int             data_checksum=0;
  int             header_checksum=0;
  int             recv_checksum=0;
  int             recv_data_checksum=0;
  char                    *from = NULL, *to = NULL;
  int                      length_to_check = 0;
  int                      packing_mode = COPYING;
  int                      errorcode = ULM_SUCCESS;
  unsigned char           *start_addr;

  START_MARK;

  packing_mode = ( dtype !=NULL && dtype->layout != CONTIGUOUS 
      && dtype->num_pairs !=0) ?  UNPACKING : COPYING ;
  header = (quadrics_coll_header_t *)mcast_buff;

  if ( packing_mode == UNPACKING)
  {
    start_addr= (unsigned char*)recv_addr - dtype->type_map[0].offset; 
  }
  else
  {
    start_addr = (unsigned char*)recv_addr;
  }

  /* if inline, get the data <= BCAST_INLINE_SIZE_SMALL */
  if ( header->sform.coll_type == BCAST_MESG_TYPE_INLINE_S )
  {
    pack_buffer(header->sform.inline_data, start_addr, 
	count, dtype, 0, size, packing_mode, 0);
  }
  else if ( header->sform.coll_type == BCAST_MESG_TYPE_INLINE_L )
  {
    pack_buffer(header->lform.inline_data, start_addr, 
	count, dtype, 0, size, packing_mode, 0);
  }
  else 
  {
    assert ( header->sform.coll_type != BCAST_MESG_TYPE_INVALID );
    if ( header->sform.coll_type == BCAST_MESG_TYPE_BUGGY )
    {
      /* if bugged inline, get the first frag <= 64 bytes */
      pack_buffer(header->lform.inline_data, start_addr, 
	  count, dtype, 0, header->sform.frag_length, 
	  packing_mode, 0);
      /* For compatability with the pack_buffer call */
      from = (char*)header->lform.data - header->sform.frag_length;
      to   = (char*)start_addr;
      length_to_check = header->sform.mesg_length - header->sform.frag_length;
    }
    else 
    {
      from = (char*)header->sform.data;
      to   = (char*)start_addr;

      length_to_check =	header->sform.mesg_length;

    }
  }

  if (DoChecksum)
  {
    if ( header->sform.coll_type == BCAST_MESG_TYPE_INLINE_L ||
        header->sform.coll_type == BCAST_MESG_TYPE_BUGGY )
    {
      header_checksum = usecrc()?
          uicrc  (mcast_buff, 128): uicsum (mcast_buff, 128);
      recv_checksum   = header->lform.checksum;
      recv_data_checksum = header->lform.data_checksum;
    }
    else
    {
      header_checksum = usecrc()?
          uicrc  (mcast_buff, 64): uicsum (mcast_buff, 64);
      recv_checksum   = header->sform.checksum;
      recv_data_checksum = header->sform.data_checksum;
    }

    if (length_to_check) 
    {
      int offset = (header->sform.coll_type == BCAST_MESG_TYPE_BUGGY )
	? header->sform.frag_length : 0;
      data_checksum = pack_buffer(from, to, count, dtype, offset, 
	    length_to_check, packing_mode, 1);
    }

    if ( usecrc())
    {
      errorcode = ((data_checksum == recv_data_checksum) && 
	( header_checksum == 0 ) )?
	ULM_SUCCESS : ULM_ERR_BCAST_RECV;
    }
    else
      errorcode = ((data_checksum == recv_data_checksum) && 
	( header_checksum == (recv_checksum + recv_checksum)))?
	ULM_SUCCESS : ULM_ERR_BCAST_RECV;

#if 1
    if ( errorcode != ULM_SUCCESS)
    {
      fprintf(stdout, "DataChecksum: Recv %x computed %x\n", 
	  recv_data_checksum, data_checksum);
      fflush(stdout);
    }
#endif
  }
  else 
  if ( length_to_check)
  {
    int offset = (header->sform.coll_type == BCAST_MESG_TYPE_BUGGY )
      ? header->sform.frag_length : 0;
    pack_buffer(from, to, count, dtype, offset, length_to_check,
	packing_mode, 0);
  }

  END_MARK;
  return errorcode;
}

/* The receivers in the broadcast operations */
int Broadcaster::bcast_recv(quadrics_channel_t * channel)
{
  Group       * localGroup;
  int           errorcode = ULM_SUCCESS;
  int           index = channel->index;

  ELAN3_CTX             *ctx;
  quadrics_coll_header_t * header;
  header = (quadrics_coll_header_t*)channel->mcast_buff;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;
#ifdef ENABLE_RELIABILITY
  /* Take the timestamp of the ulm_request */
  if (channel->repeats < 1 )
    channel->time_started = 
      ((bcast_request_t *)channel->ulm_request)->time_started;
  else
  {
    channel->time_started = dclock();
    channel->repeats ++;
    if ( channel->repeats == 10) 
    {
      communicators[comm_index]->hw_bcast_enabled = 0;
      return ULM_ERR_BCAST_FAIL;
    }
  }
#endif 

  localGroup = communicators[comm_index]->localGroup;
  if ( 
      /* I am not the comm_root for this host */
      (localGroup->mapGroupProcIDToHostID[channel->root] == 
       localGroup->mapGroupProcIDToHostID[self] ) ||
     (localRoot != self))
  {
     /* Just mark the channel as pseudo completed */
     channel->status = ULM_STATUS_COMPLETE;
     channel->send_mode = BCAST_USMP_CHANNEL;
  }
  else 
  {
#ifdef ENABLE_RELIABILITY
    double   time_now = 0;
    volatile E3_int32 * done_recv = 
      (E3_int32*)& ctrl->recv_blk_main[index]->eb_done;
    while (!EVENT_WORD_READY(done_recv) && 
	  (time_now < channel->time_started + BCAST_RECV_TIMEOUT))
      time_now = dclock();

    if (time_now >= channel->time_started + BCAST_RECV_TIMEOUT)
      errorcode = ULM_ERR_BCAST_RECV;
#else
    /* Recv is always blocking? */
    elan3_waitevent_blk (ctx, ctrl->glob_event[index],
       	ctrl->recv_blk_main[index], quadrics_Glob_Mem_Info->waitType);
#endif

    /* Does the message has the expected tag, etc? */
    if ((channel->tag != header->sform.tag_m )|| 
	(header->sform.mesg_length != 
	(int)channel->count * channel->data_type->packed_size))
      errorcode = (errorcode == ULM_SUCCESS)? ULM_ERR_BCAST_RECV : errorcode;

    /* Copy away and CRC checking */
    if ( errorcode == ULM_SUCCESS )
    {
      errorcode = bcast_unpack_after_dma(
	  channel->mcast_buff, 
	  channel->appl_addr, 
	  channel->count, 
	  channel->data_type, 
	  header->sform.mesg_length,quadricsDoChecksum);
    }

    if ( errorcode != ULM_SUCCESS )
    {
      faulted   = 1;
      E3_WRITE_DMA_DESTEVENT(ctx, sync->sync_dma_elan, 
	  S2E(sync->nack_event[index])); 
      elan3_putdma(ctx, sync->sync_dma_elan);
      return errorcode;
    }
    else
    {
      /* Mark the sender as completed */
      channel->ulm_request->messageDone = true;

      E3_RESET_BCOPY_BLOCK(ctrl->recv_blk_main[index]);
      elan3_primeevent(ctx, ctrl->glob_event[index], 1);
    }
  }

  END_MARK;
  return ULM_SUCCESS;
}

/* The root of a broadcast operation does a bcast_send */
int Broadcaster:: bcast_send(quadrics_channel_t * channel)
{
  Group               *localGroup;

  int                  length;
  int                  elan_bugged = 0;     /* bugged        */
  int                  inbounds;            /* within bounds */
  int                  copying = 0;         /* need copying  */
  int                  packing = COPYING;   /* need packing  */
  int                  num_dma;             /* how many dma's  */
  maddr_vm_t           addr;
  bcast_segment_t     *temp;
  int                  index; 
  int                  slot_offset = 0; 
  int                  count ;
  ULMType_t           *dtype;

  quadrics_coll_header_t        * mesg_env;

  ELAN3_CTX             *ctx;
  START_MARK;

#ifdef ENABLE_RELIABILITY
  /* Take the timestamp of the ulm_request */
  if (channel->repeats < 1 )
    channel->time_started = 
      ((bcast_request_t *)channel->ulm_request)->time_started;
  else
  {
    channel->time_started = dclock();
    channel->repeats ++;
    if ( channel->repeats == 10) 
    {
      communicators[comm_index]->hw_bcast_enabled = 0;
      return ULM_ERR_BCAST_FAIL;
    }
  }
#endif 

  ctx        = quadrics_Glob_Mem_Info->ctx;
  localGroup = communicators[comm_index]->localGroup;
  dtype      = channel->data_type;
  count      = channel->count;
  index      = channel->index;
  addr       = channel->appl_addr;
  length     = count * dtype->packed_size;

  /* When the data is not contiguous type */
  if (dtype != NULL && dtype->layout != CONTIGUOUS && dtype->num_pairs !=0)
  {
    /* Message fragmentation is not supported by Broadcaster */
    packing = PACKING;
  }

  inbounds   = ELAN3_ADDRESSABLE (ctx, addr, length) ? 1 : 0;

  /* Does the data need to be buffered? */
  copying = ((length <= BCAST_SMALLMESG ) || (!inbounds) ) ? 1 : 0 ;

  /* 
   * If data is not to be packed or copied and 
   * it starts from a bugging address,
   * care must be taken for the elan bug.
   */
  if ( inbounds)
    elan_bugged = (E3_DMA_REVB_BUG_2 (length, M2E(addr), page_size) 
	&& (!copying) && (!packing)) ? 1 : 0 ;

  /* Currently use at most 2 dma for one message */
  num_dma = (copying || packing )? 1 : 2 ;
  mesg_env = (quadrics_coll_header_t*) channel->mcast_buff;

  /* Consider moving this to somewhere else */
  memset((void*) mesg_env, 0, 128);

  /* All these should be done here */
  mesg_env->sform.comm_index = comm_index; 
  mesg_env->sform.desc_index = channel->seqno; 
  mesg_env->sform.group_index= localGroup->groupID; 
  mesg_env->sform.root  = channel->root;       
  mesg_env->sform.tag_m = channel->tag;      

  bcast_pack_for_dma(addr, channel->mcast_buff, 
      count, dtype, packing, copying, elan_bugged, quadricsDoChecksum, 1);

  if ( num_dma == 1)  /* copying or packing.  bcast from mcast_buff */
  {
    slot_offset = index * MAX_BCAST_FRAGS;
    temp = segment; 
    do
    {
      E3_WRITE_DMA_SIZE(ctx, temp->dma_elan+slot_offset * sizeof(E3_DMA_MAIN), 
	  mesg_env->sform.data_length);
      if ( !temp->next) 
	break;
      else
      {
	elan3_initevent(ctx, temp->event + slot_offset*sizeof(E3_Event)); 
	elan3_waitdmaevent(ctx, 
	    temp->next->dma_elan + slot_offset * sizeof(E3_DMA_MAIN), 
	    temp->event+ slot_offset*sizeof(E3_Event)); 
      }
      temp = temp->next;
    } 
    while(temp);

    elan3_putdma(ctx, segment->dma_elan + slot_offset * sizeof(E3_DMA_MAIN));
  }
  else  /* bcast from user buffer */
  {
    maddr_vm_t buff_location;
    int        dma_size; 
    E3_Addr    dma_source, dma_dest ;

    temp = segment; 
    slot_offset = index * MAX_BCAST_FRAGS + 1;

    buff_location = elan_bugged ? 
      ((char *)channel->appl_addr + mesg_env->sform.frag_length):
      ((char *)channel->appl_addr); 

    /* Send from the source, start at a new page if bugged */
    dma_source    = M2E(buff_location);
    dma_dest      = elan_bugged ? 
      M2E(mesg_env->lform.data): M2E(mesg_env->sform.data);
    dma_size      = mesg_env->sform.data_length  - ( elan_bugged ? 128 : 64);

    do
    {
      E3_WRITE_DMA_SOURCE(ctx, 
	  temp->dma_elan+slot_offset *sizeof(E3_DMA_MAIN), (dma_source));
      E3_WRITE_DMA_DEST(ctx, 
	  temp->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), (dma_dest));
      E3_WRITE_DMA_SIZE(ctx, 
	  temp->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), dma_size);
      if ( !temp->next) 
	break;
      else
      {
	elan3_initevent(ctx, temp->event+slot_offset*sizeof(E3_Event)); 
	elan3_waitdmaevent(ctx, 
	    temp->next->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), 
	    temp->event+slot_offset*sizeof(E3_Event)); 
      }
      temp = temp->next;
    } 
    while(temp);

    E3_WRITE_DMA_SRCEVENT(ctx, temp->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), 
      S2E(temp->event+ slot_offset*sizeof(E3_Event))); 

    /* Chain the first list of dma with the second list */
    elan3_initevent(ctx, temp->event+slot_offset*sizeof(E3_Event)); 
    elan3_waitdmaevent(ctx, 
	segment->dma_elan+(slot_offset - 1)*sizeof(E3_DMA_MAIN), 
	temp->event+slot_offset*sizeof(E3_Event)); 

    /* Do the second round */
    /*slot_offset = slot_offset - 1;*/
    slot_offset = index * MAX_BCAST_FRAGS;
    temp = segment;
    dma_size      = ( elan_bugged ? 128 : 64);
    do
    {
      E3_WRITE_DMA_SIZE(ctx, 
	  temp->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), dma_size);
      if ( !temp->next) 
	break;
      else
      {
	elan3_initevent(ctx, temp->event+slot_offset*sizeof(E3_Event)); 
	elan3_waitdmaevent(ctx, 
	    temp->next->dma_elan+slot_offset*sizeof(E3_DMA_MAIN), 
	    temp->event+slot_offset*sizeof(E3_Event)); 
      }
      temp = temp->next;
    } 
    while(temp);

    elan3_putdma(ctx, 
	segment->dma_elan + (slot_offset +1)*sizeof(E3_DMA_MAIN));

#ifdef USE_ELAN_SHARED
    /* 
     * Now copy the data into mcast_buff to distribute to smp processors 
     * on this node, do this copy here so we do not delay starting 
     * the dma above.  
     *
     * set copying==1 to force a copy
     * disable CRC - dont need it for shared memory
     * disable filling in DMA headers
     */
    bcast_pack_for_dma(addr, channel->mcast_buff, 
                       count, dtype, packing, 1, elan_bugged, 0, 0);
#endif

  }

  /* If buffered sending, update the channel , 
   * and mark the sender as completed */
  if (copying)
  {
    channel->send_mode = BCAST_BUFF_CHANNEL;
    channel->ulm_request->messageDone = true;
  }

  END_MARK;
  return ULM_SUCCESS;
}

/* all processes update their channels index to get synchronized */
void 
Broadcaster::update_channels(int next_toack)
{
  int i, j, index;

  ELAN3_CTX             *ctx;
  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;

  i = next_toack - 1;

  /* Reinitialize the control events for root processes */
  for ( j = ack_index; j < next_toack ; j++)
  {
    index = j % total_channels;
    if ( channels[index]->root == self )
    {
      /* The transmission structures for local master,
       * not needed until we do non-blocking dma */
      E3_RESET_BCOPY_BLOCK(ctrl->recv_blk_main[index]);
      elan3_primeevent(ctx, ctrl->glob_event[index], 1);
    }
  }

  /* Reinitialize the synchronization events */
  /*for ( i = ack_index; i < next_toack ; i++)*/
  {
    index = i % total_channels;
    channels[index]->status = ULM_STATUS_COMPLETE;

    if ( channels[index]->send_mode == BCAST_SEND_CHANNEL )
      channels[index]->ulm_request->messageDone = true;
    if ( self_vp == master_vp )
    {
      /* The transmission structures for master */
      E3_RESET_BCOPY_BLOCK(sync->recv_blk_main[index]);
      elan3_primeevent(ctx, sync->glob_event[index], (num_branches +1));
    }
    /* If I am the local root */
    else if ( self_vp == local_master_vp )
    {
      /* The synchronization structures */
       elan3_waitdmaevent0(ctx, 
           (sync->chain_dma_elan + index*sizeof(E3_DMA_MAIN)),
           (sync->glob_event[index]),
           (sync->src_wait_ack+index*sizeof(E3_WaitEvent0)), 
           (num_branches +1));
    }
    else if ( channels[index]->root == self )
    {
      /*E3_RESET_BCOPY_BLOCK(sync->recv_blk_main[index]);*/
      /*elan3_primeevent(ctx, sync->glob_event[index], (num_branches +1));*/
    }
  }

  /* remove the ulm requests */
  for ( i = ack_index; i < next_toack ; i++)
  {
    bcast_request_t * temp_req = 
	(bcast_request_t*)channels[ i % total_channels ]->ulm_request;
    communicators[comm_index]->free_bcast_req(temp_req);
  }
  /* update the number of descriptors */
  desc_avail += ((next_toack) - ack_index);
  ack_index = next_toack;
  faulted   = 0;
  END_MARK;
}

/* Function to check outstanding channels */
int 
Broadcaster::check_channels()
{
  int                  toack, toqueue;
  int                  errorcode = ULM_SUCCESS; 
  int                  i=0, index=0, root_id=0;
  Group               *localGroup;
  double               time_now = 0;
  int                  timed_out = 0; 

  ELAN3_CTX             *ctx;

  START_MARK;
  ctx        = quadrics_Glob_Mem_Info->ctx;

  localGroup = communicators[comm_index]->localGroup;
  toack = ack_index;
  toqueue = queue_index;

  assert ( toack + total_channels>= toqueue);
  if (toack == toqueue) 
  {
    END_MARK;
    return ULM_SUCCESS;
  }

#ifdef ENABLE_RELIABILITY
  if ( faulted )
    return ULM_ERR_BCAST_SYNC;
#endif
  {
    for ( i = toack ; i < toqueue ; i++)
    {
      index = i % total_channels;

      /* Need to check all the outstanding send channels */
      if ( channels[index]->send_mode == BCAST_SEND_CHANNEL )
      {
#ifdef ENABLE_RELIABILITY
	volatile E3_int32 * done_word;
	done_word = (volatile E3_int32 *)	
	  & ctrl->recv_blk_main[index]->eb_done;
	while (!(EVENT_WORD_READY(done_word)) && 
	    time_now < channels[index]->time_started + BCAST_SEND_TIMEOUT)
	  time_now = dclock();
	if (time_now >= channels[index]->time_started + BCAST_SEND_TIMEOUT)
	{ 
	  timed_out = 1;
	  return ULM_ERR_BCAST_SYNC;
	}
#else
	elan3_waitevent_blk (ctx, ctrl->glob_event[index], 
	  ctrl->recv_blk_main[index], quadrics_Glob_Mem_Info->waitType);
#endif
      }
    }

    index = (toqueue -1 ) % total_channels;
    root_id = channels[index]->root ;

    if ( self == root_id  ||
	((localGroup->mapGroupProcIDToHostID[root_id] != 
	 localGroup->mapGroupProcIDToHostID[self] ) &&
	(self_vp == local_master_vp )))
    {
#ifdef ENABLE_RELIABILITY
      if (timed_out)
	E3_WRITE_DMA_DESTEVENT(ctx, sync->sync_dma_elan, 
	  S2E(sync->nack_event[index])); 
      else
#endif
      {
	E3_WRITE_DMA_DESTEVENT(ctx, sync->sync_dma_elan, 
	  S2E(sync->glob_event[index])); 
      }
      elan3_putdma(ctx, sync->sync_dma_elan);
    }
  }
  return ULM_SUCCESS;
}


/* The manager of a broadcaster (per communicator) does this 
 * to check outstanding broadcaster operations and
 * synchronize all others with a broadcast
 */
int 
Broadcaster::sync_parent(int errorcode)
{
  int                  toack, toqueue, next_toack;
  int                  i, index, root_id;
  double               time_now = 0; 
  int                  timed_out = 0; 
  E3_DMA              *sync_dma;
  E3_Event_Blk        *blk;
  Group               *localGroup;

  /* The master checked the current status of outstanding channels,
   * then it updates others about its conclusion */
#ifdef ENABLE_RELIABILITY
  int nack_detected = 0;
#endif

  ELAN3_CTX             *ctx;

  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;
  localGroup = communicators[comm_index]->localGroup;
  next_toack = toack = ack_index;
  toqueue = queue_index;

  sync_dma = sync->sync_dma;

  /*toack  = ((ulm_coll_env_t*) sync->glob_env)->desc_index;*/
  i = toqueue - 1;
  index = i % total_channels;
  blk = sync->recv_blk_main[index];

#ifdef ENABLE_RELIABILITY
  {
    volatile E3_int32 * sync_word;
    volatile E3_int32 * nack_word;
    sync_word = (volatile E3_int32 *)	& blk->eb_done;

    while (!(EVENT_WORD_READY(sync_word)) && ! nack_detected && !timed_out)
    {
      /* If timed out or nack detected */
      for ( i = toack ; i < toqueue ; i++)
      {
	/* Need to check all the outstanding send channels */
	nack_word = (volatile E3_int32 *)	
	  & sync->nack_blk_main[i%total_channels]->eb_done;

	if (EVENT_WORD_READY(nack_word)) 
	  nack_detected = 1;
      }
      time_now = dclock();
      if (time_now >= channels[index]->time_started + BCAST_SYNC_TIMEOUT)
	timed_out = 1;
    }
  }

  if ( nack_detected || timed_out)
  {
    ((ulm_coll_env_t*) sync->glob_env)->coll_type = BCAST_MESG_TYPE_NACK;
  }
  else
  {
    /* Here we are only sync on the last outstanding channel,
     * If there is anything wrong encountered, all the outstanding
     * channels would be marked as incomplete.
     */
    ((ulm_coll_env_t*) sync->glob_env)->desc_index  = toqueue;
    ((ulm_coll_env_t*) sync->glob_env)->coll_type = BCAST_MESG_TYPE_SYNC;
  }
#else

  elan3_waitevent_blk (ctx, sync->glob_event[index], 
      blk, quadrics_Glob_Mem_Info->waitType);
  ((ulm_coll_env_t*) sync->glob_env)->desc_index  = toqueue;
  ((ulm_coll_env_t*) sync->glob_env)->coll_type = BCAST_MESG_TYPE_SYNC;
#endif

  {
    bcast_segment_t * temp;
    int slot_offset;
    temp = segment; 

    slot_offset = (total_channels * MAX_BCAST_FRAGS );
    do
    {
      if ( !temp->next) 
	break;
      else
      {
	elan3_initevent(ctx, temp->event + slot_offset*sizeof(E3_Event)); 
	elan3_waitdmaevent(ctx, 
	    temp->next->dma_elan + slot_offset * sizeof(E3_DMA_MAIN), 
	    temp->event+ slot_offset*sizeof(E3_Event)); 
      }
      temp = temp->next;
    } 
    while(temp);
    /*E3_WRITE_DMA_SRCEVENT(ctx, temp->dma_elan, S2E(sync->src_event));*/
    elan3_putdma(ctx, segment->dma_elan+slot_offset * sizeof(E3_DMA_MAIN));
  }

  elan3_waitevent_blk (ctx, sync->src_event,
      sync->src_blk_main, quadrics_Glob_Mem_Info->waitType);
  E3_RESET_BCOPY_BLOCK(sync->src_blk_main);
  elan3_primeevent(ctx, sync->src_event, 1);

  /* Reset the index */
  ((ulm_coll_env_t*) sync->glob_env)->desc_index  = 0;

#ifdef ENABLE_RELIABILITY
  if ( nack_detected || timed_out)
  {
    /* Complete all bcast_request_t upto toqueue */
    return ULM_ERR_BCAST_SYNC;
  }
#endif
 
  END_MARK;
  return ULM_SUCCESS;
}

/* 
 * All non-manager processes does this to synchronize with others
 * throught the manager processes.
 * A root of a broadcaster has to call this if it is not the manager.
 */
int 
Broadcaster::sync_leaves(int errorcode)
{
  int                  toack, toqueue;
  int                  i=0, index=0, root_id=0;
  Group               *localGroup;
  double               time_now = 0;
  int                  timed_out = 0; 

  ELAN3_CTX             *ctx;

  START_MARK;
  ctx        = quadrics_Glob_Mem_Info->ctx;
  localGroup = communicators[comm_index]->localGroup;
  toack = ack_index;
  toqueue = queue_index;

  /* The local_master waits for the conclusion from the master */
  if (self_vp == local_master_vp )
  {
    volatile E3_uint32 *new_ack;
    int                 next_ack;
    volatile E3_uint32 *coll_type;

    time_now = 0;
    new_ack = ((E3_uint32 *)&((ulm_coll_env_t*) 
	  sync->glob_env)->desc_index);

#ifdef ENABLE_RELIABILITY
    coll_type = ((E3_uint32 *)&((ulm_coll_env_t*) 
	  sync->glob_env)->coll_type);
    index = (toqueue - 1) % total_channels;

    while (((*new_ack) <= (E3_uint32) toack )  && 
	((*coll_type) != BCAST_MESG_TYPE_NACK)  && 
	(time_now < channels[index]->time_started + BCAST_SYNC_TIMEOUT))
      time_now = dclock();

    if ((time_now >= channels[index]->time_started + BCAST_SYNC_TIMEOUT) ||
	(*coll_type)==BCAST_MESG_TYPE_NACK)
    {
      /* Notify the error */
      return ULM_ERR_BCAST_SYNC;
    }
    else
    {
      next_ack = *new_ack;
      *new_ack = 0;
    }
#else
    while (((*new_ack) < (E3_uint32) toqueue))
	;
    next_ack = *new_ack;
    *new_ack = 0;
#endif
  }
  /* neither local master nor masters */
  else
  {
#ifndef USE_ELAN_SHARED
    ((ulm_coll_env_t*) sync->glob_env)->desc_index = 0;
#endif
  }

  END_MARK;
  return ULM_SUCCESS;
}

#ifdef ENABLE_RELIABILITY
/* A function to retransmit the outstanding broadcast when the
 * hw/bcast is still available */
int 
Broadcaster::resync_bcast()
{
  int errorcode = ULM_SUCCESS;
  int i, index ;

  ELAN3_CTX             *ctx;
  START_MARK;
  ctx         = quadrics_Glob_Mem_Info->ctx;

#if 1
  /* Synchronization, use ulm_barrier().*/
  ulm_barrier(comm_index);
  init_coll_events(ack_index, queue_index-1, 0);
  ulm_barrier(comm_index);

  /* retransmission with pt2pt */
  return ULM_ERR_BCAST_MOVE;

  /* retransmission with hw/bcast */
  for ( i = ack_index ; i < queue_index; i++)
  {
    index = i % total_channels;
    if (channels[index]->root == self)
      errorcode = bcast_send(channels[index]);
    else {
      errorcode = bcast_recv(channels[index]);
    }
  }
 
  errorcode = check_channels();

  /* The master process */
  if (self_vp == master_vp )        
  {
    errorcode = sync_parent(errorcode);
  }
  /* The children */
  else                             
  {
    errorcode = sync_leaves(errorcode);
  }
#endif
  return errorcode;
}
#endif

int 
Broadcaster::make_progress_bcast()
{
  int errorcode = ULM_SUCCESS;

  START_MARK;

  ELAN3_CTX             *ctx;
  ctx         = quadrics_Glob_Mem_Info->ctx;
  errorcode   = check_channels();

  /* The master process */
  if (self_vp == master_vp )        
  {
    errorcode = sync_parent(errorcode);
  }
  else                             /* The children */
  {
    errorcode = sync_leaves(errorcode);
  }

#ifdef ENABLE_RELIABILITY
  while ( errorcode != ULM_SUCCESS && errorcode != ULM_ERR_BCAST_FAIL)
  {
    double time_now = 0;
    double time_last_req ; 

    /* A retransmission function */
    errorcode = resync_bcast();

    if (errorcode == ULM_ERR_BCAST_MOVE)
    {
      /* When leaving this function, no more chances to update the
       * channels. Do it now. */
      update_channels(queue_index);
      return errorcode;
    }
    else if ( errorcode != ULM_SUCCESS)
    {
      time_now = dclock();
      time_last_req = channels[ack_index %total_channels]->time_started;
      if (time_now >=  time_last_req + BCAST_SYNC_TIMEOUT )
      {
	/* Update anyway, although they are not to be used again*/
	update_channels(queue_index);

	/* a communicator function needs to complete the 
	 * outstanding bcast_request's */
	communicators[comm_index]->hw_bcast_enabled = 0;
	errorcode = ULM_ERR_BCAST_FAIL;
      }
    }
  }

#endif

  /* Update the channels to release the ulm_request's */
  if (errorcode == ULM_SUCCESS)
    update_channels(queue_index);

  END_MARK;
  return errorcode;
}

/* 
 * ===================================================================== 
 * ================== Communicator functions  ==========================
 * ===================================================================== 
 */

/* 
 * A function to bind a bcast request to a channel.
 * If no channels available, trigger the progress engine 
 */
int 
Communicator::bcast_bind(bcast_request_t * ulm_req, int self, 
    int root, int comm_root, void * buf, size_t size, 
    ULMType_t *dtype, int tag, int comm, 
    CollectiveSMBuffer_t *coll_desc, int sendMode)
{
    quadrics_channel_t * channel;
    int rc, assigned_index ;

    ELAN3_CTX             *ctx;

    START_MARK;
    ctx         = quadrics_Glob_Mem_Info->ctx;

    /* make sure that the request object is in the inactive state */
    if (ulm_req->status == ULM_STATUS_INVALID) 
    {
        ulm_err(("Error: ULM request structure has invalid status\n"
                 "  Status :: %d\n", ulm_req->status));
      END_MARK;
      return ULM_ERR_BCAST_FAIL;
    }

#ifdef ENABLE_RELIABILITY
    /* 
     * Trigger the progress, before running out of channels.
     */
    while (bcaster->desc_avail <= (bcaster->total_channels /2)) 
    {
      rc = bcaster->make_progress_bcast();
      if ( rc != ULM_SUCCESS);
      {
	bcast_request_t * prev = (bcast_request_t*)ulm_req->prev; 
	fail_over(prev);
	return rc;
      }
    }
#else
    while ( bcaster->desc_avail <= (bcaster->total_channels /2 ))
      bcaster->make_progress_bcast();
#endif 

    /* Save the information into ulm_req */
    {
      ulm_req->ready_toShare  = false;
      ulm_req->messageDone    = false;

      if ( self == root ) /* Make sure the sender is also a comm_root */
	ulm_req->requestType = REQUEST_TYPE_SEND;
      else if ( self == comm_root )
	ulm_req->requestType = REQUEST_TYPE_RECV;
      else 
	ulm_req->requestType = REQUEST_TYPE_SHARE;

      // set pointer to datatype
      ulm_req->datatype      = dtype;
      ulm_req->datatypeCount = size;
      ulm_req->root          = root;
      ulm_req->comm_root     = comm_root;

      // set buf type, posted size
      if (dtype == NULL) {
	 ulm_req->posted_m.length_m = size;
      } else {
	 ulm_req->posted_m.length_m = dtype->packed_size * size;
      }

      // set SMBuffer descriptor
      ulm_req->coll_desc = coll_desc;

      // set send address, leave the initial parameter there
      ulm_req->posted_buff = buf;
      ulm_req->posted_m.proc.source_m      = root;
      
      // set user_tag
      ulm_req->posted_m.UserTag_m = tag;

      // set communicator
      ulm_req->ctx_m = comm;

      // set request state to inactive
      ulm_req->status = ULM_STATUS_INITED;

      // set the send mode
      ulm_req->sendType = sendMode;

      // set the persistence flag
      //ulm_req->persistent = false;
    }

    /* Pass the information to a channel.  Bad duplication.
     * Consider avoiding the duplication */
    {
      /* Get the entry */
      assigned_index = bcaster->queue_index;
      channel = bcaster->channels[ assigned_index % bcaster->total_channels];

      /* hook the channel and the request */
      ulm_req->channel       = (void *) channel;
      channel->ulm_request   = ulm_req;

      /* Initialize the fields */
      channel->comm          = ulm_req->ctx_m;
      channel->status        = ulm_req->status;
      channel->tag           = ulm_req->posted_m.UserTag_m;
      channel->data_type     = ulm_req->datatype;
      channel->comm_type     = ulm_req->requestType;
      channel->root          = ulm_req->posted_m.proc.source_m;
      channel->count         = ulm_req->datatypeCount;
      ulm_req->seqno         =
      channel->seqno         = bcaster->queue_index;

      if ((dtype != NULL) && (dtype->layout != CONTIGUOUS) 
	  && (dtype->num_pairs != 0)) 
      {
	channel->appl_addr     = 
	    (void *)((char *)buf + dtype->type_map[0].offset);
      } 
      else 
      {
	channel->appl_addr     = ulm_req->posted_buff;
      }

      channel->send_mode     = 
	(ulm_req->requestType == REQUEST_TYPE_SEND)?
	BCAST_SEND_CHANNEL : BCAST_RECV_CHANNEL;

      /* update the index and available channels */
      bcaster->queue_index ++ ;
      bcaster->desc_avail  --;
    }

    END_MARK;
    return ULM_SUCCESS;
}

int Communicator::ibcast_start( bcast_request_t * ulm_req)
{
  quadrics_channel_t * channel;

  START_MARK;
  channel = (quadrics_channel_t*)ulm_req->channel;

#ifdef ENABLE_RELIABILITY
  /* Record the timestamp. */
  ulm_req->time_started = dclock(); 
#endif

  /* Update the status as started but incomplete */
  ulm_req->status = ULM_STATUS_INCOMPLETE ;

  if ( ulm_req->requestType  == REQUEST_TYPE_SEND)
  {
    /* Sender triggers the broadcast dma, then it is ready to share */
    bcaster->bcast_send(channel);
    ulm_req->ready_toShare = true;
  }
  else if ( ulm_req->requestType  == REQUEST_TYPE_RECV)
  {
    /* a comm_root is not ready until it receives the data */
    ulm_req->ready_toShare = false;
  }
  else 
  {
    /* Other processes just wait to have the data */
    ulm_req->ready_toShare = true;
  }

  END_MARK;
  return    ULM_SUCCESS;
}

int Communicator::bcast_wait( bcast_request_t * ulm_req)
{
  quadrics_channel_t * channel;
  int total_hosts, total_procs;
  int rc = ULM_SUCCESS;
  channel = (quadrics_channel_t*)ulm_req->channel;

  START_MARK;

  // recieve processes: wait for DMA to complete:
  if ( ulm_req->requestType  == REQUEST_TYPE_RECV
       || ulm_req->requestType  == REQUEST_TYPE_SHARE) {
      rc = bcaster->bcast_recv(channel);
  }

#ifdef ENABLE_RELIABILITY
  /* Must sync before sharing. To avoid the situation
   * when the other processes are waiting to share the data,
   * but comm_root is waiting for the retransmission of the data 
   * which is to be done only after a sync.
   */
  /*if ((channel->count * channel->data_type->packed_size)> BCAST_SMALLMESG)*/
  {
    while ((bcaster->ack_index < bcaster->queue_index )&& hw_bcast_enabled)
    {
      rc = bcaster->make_progress_bcast();
      if ( rc != ULM_SUCCESS )
	rc = fail_over(ulm_req);
    }
  }

  if ( rc == ULM_SUCCESS)
    ulm_req->ready_toShare = true;
  else
    return ULM_ERR_BCAST_FAIL;
#endif

  rc = ulm_get_info(ulm_req->ctx_m, ULM_INFO_NUMBER_OF_HOSTS, 
      &total_hosts, sizeof(int));
  rc = ulm_get_info(ulm_req->ctx_m, ULM_INFO_NUMBER_OF_PROCS, 
      &total_procs, sizeof(int));

#ifdef USE_ELAN_SHARED
  if ( total_procs != total_hosts ) {
      if (bcaster->self == ulm_req->root
          || (ulm_req->comm_root != ulm_req->root
              && bcaster->self == bcaster->localRoot) ) {
          // signal data is ready for other SMP processes to start copying:
          wmb();
          ulm_req->coll_desc->flag = 2;
      }else{
          // just copy data out of shared buffer when ready:
          quadrics_coll_header_t * header;
          header = (quadrics_coll_header_t*)channel->mcast_buff;
          ULM_SPIN_AND_MAKE_PROGRESS(ulm_req->coll_desc->flag != 2);
          bcast_unpack_after_dma(channel->mcast_buff, 
	      ulm_req->posted_buff, /*channel->appl_addr, */
	      channel->count, 
	      channel->data_type, 
	      header->sform.mesg_length,0);
      }
      smpBarrier(barrierData);
  }
#else
  /* To disperse the data on a SMP box */
  if ( total_procs != total_hosts )
  {
    size_t bcast_size = 0;
    size_t copy_buffer_size = 0;
    size_t shared_buffer_size = 0;
    size_t offset = 0;
    size_t ti = 0;
    size_t mi = 0;
    size_t mo = 0;
    void *RESTRICT_MACRO shared_buffer;

    ULMType_t *type = ulm_req->datatype;
    void *buff = ulm_req->posted_buff;
    int count = ulm_req->datatypeCount;

    bcast_size = count * type->extent;

    /* set up collective descriptor, shared buffer */
    shared_buffer = ulm_req->coll_desc->mem;
    shared_buffer_size = (size_t) ulm_req->coll_desc->max_length;
    copy_buffer_size = (bcast_size < shared_buffer_size) ? 
	bcast_size : shared_buffer_size;

    smpBarrier(barrierData);
    while (ti < (size_t) count) {
        offset = 0;
        /*if (bcaster->self == ulm_req->comm_root) */
        if (bcaster->self == ulm_req->root
           || (ulm_req->comm_root != ulm_req->root
             && bcaster->self == bcaster->localRoot) )
       {
            type_pack(TYPE_PACK_PACK, shared_buffer, copy_buffer_size,
      		&offset, buff, count, type, &ti, &mi, &mo);
            wmb();
            ulm_req->coll_desc->flag = 2;
        } else {
            ULM_SPIN_AND_MAKE_PROGRESS(ulm_req->coll_desc->flag != 2);
            type_pack(TYPE_PACK_UNPACK, shared_buffer, copy_buffer_size,
      		&offset, buff, count, type, &ti, &mi, &mo);
        }

      if (ti != (size_t) count) {
	  smpBarrier(barrierData);
	  ulm_req->coll_desc->flag = 1;
	  smpBarrier(barrierData);
      }
    }
  }
#endif

  /* Record that the message is already in the application buffer */
  if ( !ulm_req->messageDone )
    ulm_req->messageDone = true;

#ifndef ENABLE_RELIABILITY
  /* Sync after sharing */
  if ((channel->count * channel->data_type->packed_size)> BCAST_SMALLMESG)
  {
    while (bcaster->ack_index < bcaster->queue_index && hw_bcast_enabled)
    {
        rc = bcaster->make_progress_bcast();
    }
  }
#endif

  if ( rc == ULM_SUCCESS)
    ulm_req->status = ULM_STATUS_COMPLETE ;

  END_MARK;
  return ULM_SUCCESS;
}

/* 
 * Restart the broadcast operation over ulm_bcast_interhost.
 * Since we do not do non-blocking broadcast operation when * reliability 
 * is turned on, using ulm_bcast_interhost is just fine here.
 */
static int bcast_restart (bcast_request_t * ulm_req)
{
  void * buff ;
  int count; 
  ULMType_t *type; 
  int root; 
  int comm;
  int total_length;
  int rc = ULM_SUCCESS;

  START_MARK;
  quadrics_channel_t * channel;
  quadrics_coll_header_t * header;
  channel = (quadrics_channel_t*)ulm_req->channel;

  buff  = ulm_req->posted_buff;
  count = ulm_req->datatypeCount ;
  type  = ulm_req->datatype;
  root  = ulm_req->posted_m.proc.source_m;
  comm  = ulm_req->ctx_m;

  total_length = count * type->packed_size; 

  /* For large size message, we can still use 
   * the application buffer */
  if ( total_length > BCAST_SMALLMESG)
  {
    END_MARK;
    return ulm_bcast_interhost(buff, count, type, root, comm);
  }

  if (ulm_req->messageDone)
  {
    header = (quadrics_coll_header_t*) channel->mcast_buff;
    count  = total_length;
    type   = (ULMType_t*)MPI_CHAR;
    if ( total_length <= BCAST_INLINE_SIZE )
      buff   = (void *) header->sform.inline_data;
    else
      buff   = (void *) header->sform.data;
  }

  rc = ulm_bcast_interhost(buff, count, type, root, comm);
  END_MARK;
  return rc; 
}

/* 
 * Have the incomplete broadcast fail-over to point-to-point cases
 */
int Communicator::fail_over(bcast_request_t * ulm_req)
{
  bcast_request_t * temp;
  bcast_request_t * prev;
  int rc = ULM_SUCCESS;

  START_MARK;

  /* Find the first bcast_request_t to be synchronized */
  temp = bcast_queue.first;

  while (temp)
  {
    prev = temp;
    assert(temp->seqno <= ulm_req->seqno );
    rc = bcast_restart(temp);
    if (rc != ULM_SUCCESS) return rc;
    temp = (bcast_request_t*)temp->next;
    free_bcast_req(prev);
  }

  END_MARK;
  return ULM_SUCCESS;
}

#endif  /* USE_ELAN_COLL */
