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

#ifndef _ULM_COLLECTIVES_H_
#define _ULM_COLLECTIVES_H_

#include "internal/mpi.h"
#include "internal/linkage.h"
#include "ulm/types.h"


enum {NUM_COLL_OPS = 14};
enum {BARRIER, BROADCAST, GATHER, GATHERV, SCATTER, SCATTERV, ALLGATHER,
      ALLGATHERV, ALLTOALL, ALLTOALLV,
      REDUCE, ALLREDUCE, REDUCE_SCATTER, SCAN};

/* Structs to hold/track collective operations */
typedef struct coll_op_t coll_op_t;
struct coll_op_t {
    char* desc;
    void* op;
    int tag;
};

/* tree structure */
typedef struct ulm_tree {
    /* number of nodes in this tree */
    int nNodes;
    /* tree order */
    int order;
    /* tree depth */
    int treeDepth;
    /* number of full tree levels */
    int nFullLevels;
    /* number of nodes in full levels */
    int nNodesInFullLevels;
    /* nuber of up-nodes - 1 per node */
    int *nUpNodes;
    /* node id of "parent" node - 1 per node */
    int *upNodes;
    /* number of downNodes */
    int *nDownNodes;
    /* number of child nodes in last level */
    int *nInPartialLevel;
    /* level in which node starts to appear in the tree */
    int *startLevel;
    /* list of "children nodes" - order-1 per level below
     *   - 1 per node, for each full tree level, and leftovers
     *   from last level */
    int **downNodes;
} ulm_tree_t;

CDECL_BEGIN

/* Array of collective operations */
extern coll_op_t ulm_coll_ops[];

/* Lookup function to dynamically choose the appropriate collective implementation. */
int ulm_comm_get_collective(MPI_Comm comm, int key, void **func);

/* 
 * Collective definitions utilizing the underlying hardware
 * support on quadrics.
 * 
 * NOTE: There is no reliability guaranteed with the use of
 * these functions
 */
extern ulm_barrier_t ulm_barrier_quadrics;
extern ulm_bcast_t   ulm_bcast_quadrics;
extern ulm_reduce_t  ulm_reduce_quadrics;

/* 
 *Point to Point collective definitions - simple
 * and probably functional
 */
extern ulm_reduce_t ulm_reduce_p2p;
extern ulm_bcast_t ulm_bcast_p2p;

CDECL_END

#endif
