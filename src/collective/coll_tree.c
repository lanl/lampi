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



#include <stdlib.h>

#include "collective/coll_tree.h"
#include "ulm/errors.h"
#include "internal/collective.h"
#include "internal/malloc.h"

int get_parent(int node_id, int k)
{
    if (node_id % k == 0) {
	return (((int) node_id / k) -1);
    }
    else {
	return ((int) node_id / k);
    }
}

void get_children(int node_id, int num_nodes, int k,
		  int *num_children, int **children)
{
    int i;
    *num_children = (num_nodes-1 >= node_id * (k+1)) ?
	k : num_nodes - k * node_id - 1;
    *children = (int *) ulm_malloc(*num_children * sizeof(int));
    for (i = 1; i <= *num_children; i++) {
	(*children)[i-1] = node_id * k + i;
    }
}

/*
 * This routine is used to setup a tree structure.
 *   The upNode is the node above parent node
 *   The downNodes are the child nodes (order-1 per level)
 */

int setupNaryTree(int nNodes, int order, ulm_tree_t *tree)
{
    int returnValue = ULM_SUCCESS;
    int level,cLevel,cnt,rankInThisLevel;
    int nMax,depth,cumNInLevel,node,tmp,nInFullLevels;
    int *cumNodes,cum,j;
    int partialLastLevel=0;
    int nToAlloc;

    /* set some tree parameters */
    tree->nNodes=nNodes;
    tree->order=order;

    /* figure out the tree's depth */
    nMax = nNodes;
    nInFullLevels=1;
    depth = 0;
    while (nMax > 0) {
        nInFullLevels*=order;
        nMax = nMax/order;
        depth++;
    }
    tree->nFullLevels=depth;
    tree->nNodesInFullLevels=nInFullLevels;
    if ( nInFullLevels > nNodes ){
        depth++;
        tree->nNodesInFullLevels/=order;
        partialLastLevel=1;
    }
    tree->treeDepth = depth;
    /* temp array to hold cummulative data on cummulative number of
     *  nodes
     */
    cumNodes= (int *)ulm_malloc(sizeof(int)*depth);
    if( !cumNodes ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    cum=1;
    cumNodes[0]=0;
    for( level=1 ; level < depth ; level++ ) {
        cumNodes[level]=cum;
        cum*=order;
    }


    /* loop over all nodes in the tree and figure out the exchange pairs at
     *   each level
     */

    tree->nUpNodes= (int *)ulm_malloc(sizeof(int)*nNodes);
    if( !tree->nUpNodes ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    tree->upNodes= (int *)ulm_malloc(sizeof(int)*nNodes);
    if( !tree->upNodes ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    tree->nDownNodes= (int *)ulm_malloc(sizeof(int)*nNodes);
    if( !tree->nDownNodes ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    tree->downNodes= (int **)ulm_malloc(sizeof(int *)*nNodes);
    if( !tree->downNodes ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    tree->nInPartialLevel= (int *)ulm_malloc(sizeof(int)*nNodes);
    if( !tree->nInPartialLevel ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    tree->startLevel= (int *)ulm_malloc(sizeof(int)*nNodes);
    if( !tree->startLevel ) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    level=0;
    cumNInLevel=1;
    for (node = 0; node < nNodes; node++) {

        /* update level counters */
        if( node == cumNInLevel ){
            level++;
            cumNInLevel*=order;
        }

        /* figure out parent node */
        if( level != 0 ){
            tree->upNodes[node]=(node-cumNodes[level])/(order-1);
            tree->nUpNodes[node]=1;
        } else{
            /* root node */
            tree->upNodes[node]=-1;
            tree->nUpNodes[node]=0;
        }

        /* set level at which node first appears in the tree */
        tree->startLevel[node]=level;

        nToAlloc=(order-1)*(depth-(level+1));
        if( nToAlloc > 0 ) {
            tree->downNodes[node]= (int *)ulm_malloc(sizeof(int)*nToAlloc);
            if( !(tree->downNodes[node]) ) {
                return ULM_ERR_OUT_OF_RESOURCE;
            }
        }
        else {
            tree->downNodes[node] = 0;
        }
        cnt=0;
        /* compute child nodes */
        tree->nDownNodes[node]=0;
        tree->nInPartialLevel[node]=0;
        rankInThisLevel=node;
        for( cLevel=level+1; cLevel <  depth ; cLevel++ ) {
            tmp=rankInThisLevel*(order-1)+cumNodes[cLevel];
            for( j=0 ; j < (order-1) ; j++ ) {
                if( tmp >= nNodes )
                    break;
                tree->downNodes[node][cnt]=tmp;
                tree->nDownNodes[node]++;
                tmp++;
                cnt++;
                /* count number in partial layer */
                if( ( cLevel == (depth-1) ) && partialLastLevel ){
                    (tree->nInPartialLevel[node])++;
                }
            }
        }
    }                           /* end node loop */

    /* free resources */
    if( cumNodes )
        ulm_free(cumNodes);

    return returnValue;
}

/*
 * This routine is called recursively to fill in the order in
 *   which data is expected to be ordered for a given node of
 *   an arbitray order tree
 */
void getScatterNodeOrder(int rootNode, int *nodeList, int *lenNodeList,
                         int fillInNodeList, ulm_tree_t *treeData)
{
    int node;
    /* add root Node to list */
    if( fillInNodeList)
        nodeList[(*lenNodeList)]=rootNode;
    (*lenNodeList)++;

    /* loop through all child nodes, starting at the low order nodes */
    for( node=0 ; node < treeData->nDownNodes[rootNode] ; node++){
        getScatterNodeOrder(treeData->downNodes[rootNode][node],
                            nodeList,lenNodeList,fillInNodeList,treeData);
    }
}

/*
 * This routine is called recursively to fill in the gather order in
 *   which data is expected to be ordered for a given node of
 *   an arbitray order tree
 */
void getGatherNodeOrder(int rootNode, int *nodeList, int *lenNodeList,
                        int fillInNodeList, ulm_tree_t *treeData)
{
    int i,cnt,nInPartialLevel,nDownNodes,startIndex;

    /* add root Node to list */
    if( fillInNodeList)
        nodeList[(*lenNodeList)]=rootNode;
    (*lenNodeList)++;

    /* loop through all child nodes, starting at the low order nodes */
    cnt=0;
    /* partial last level */
    nInPartialLevel=treeData->nInPartialLevel[rootNode];
    nDownNodes=treeData->nDownNodes[rootNode];
    if( nInPartialLevel > 0 ) {
        /* fill in the source process */
        startIndex=nDownNodes-nInPartialLevel;
        for( i=startIndex ; i < nDownNodes; i++ ) {
            /* get host index in "shifted" host index * list */
            getGatherNodeOrder(treeData->downNodes[rootNode][i],
                               nodeList,lenNodeList,fillInNodeList,treeData);
            cnt++;
        }
    }

    while( cnt < treeData->nDownNodes[rootNode] ) {
        startIndex=nDownNodes-(cnt)-(treeData->order-1);
        for( i=startIndex ; i < startIndex+(treeData->order-1) ; i++ ) {
            /* get host index in "shifted" host index list */
            getGatherNodeOrder(treeData->downNodes[rootNode][i],
                               nodeList,lenNodeList,fillInNodeList,treeData);
            cnt++;
        }
    }

    /*
      for( node=treeData->nDownNodes[rootNode]-1 ; node >= 0 ; node--){
      getNodeOrder(treeData->downNodes[rootNode][node],
      nodeList,lenNodeList,fillInNodeList,treeData);
      }
    */
}
