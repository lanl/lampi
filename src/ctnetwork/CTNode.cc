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

/*
 *  CTNode.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Tue Jan 07 2003.
 */

#include <strings.h>

#include "CTNode.h"
#include "internal/malloc.h"
#include "util/misc.h"


#define ct_node_err(x)  do {\
    _ulm_err ("[%s:%d] ", _ulm_host(), getpid()); \
    _ulm_err ("Node %d: ", label_m); \
    _ulm_set_file_line(__FILE__, __LINE__) ; _ulm_err x ;  \
    fflush(stderr); } while (0)

#ifdef CT_DEBUG
#define ct_node_dbg(x)          ct_node_err(x)
#else
#define ct_node_dbg(x)
#endif



CTNode *CTNode::createNode(unsigned int type, unsigned int label, unsigned int numberOfNodes)
{
    CTNode          *node = NULL;
        
    switch ( type )
    {
    case kHypercube:
        node = new HypercubeNode(label, numberOfNodes);
        break;
    default:
        break;
    }
        
    return node;
}
        

CTNode::CTNode(unsigned int label, unsigned int numberOfNodes)
    : label_m(label), ntype_m(0), numNodes_m(numberOfNodes), userInfo_m(NULL)
{
    cachedScatterList_m = NULL;
}


CTNode::~CTNode()
{
}

void CTNode::setNeighborDeleteMethod(void (*del_fn)(void*))
{
    neighbors_m.setFreeFunction(del_fn);
}

void CTNode::addNeighbor(unsigned int label, void *neighbor)
{
    neighbors_m.setValueForKey(neighbor, label);
}

void *CTNode::neighbor(unsigned int label)
{
    return neighbors_m.valueForKey(label);
}




/*
 *
 *                      Hypercube implementation
 *
 */
 
/*
  The broadcasting algorithm is based on the paper by Howard Katseff:
  Incomplete Hypercubes, IEEE Trans. on Comp., Vol. 37, No. 5, May 1988.
*/


/* max number of nodes is 2^32 */
#define HCUBE_MAX_SZ            32


inline unsigned int _hamming_distance(unsigned int node1, unsigned int node2);
inline unsigned int _hamming_weight(unsigned int node);


typedef struct
{
    unsigned int    **_list;
    unsigned int    *_listCounts;
    unsigned int    *_labels;
    unsigned int    cnt_m;
} _cache_item_t;


typedef struct
{
    unsigned int    **_list;
    unsigned int    *_links;
    unsigned int    *_listCounts;
    unsigned int    cnt_m;
} _scatter_t;


static void _free_cache_item(void *arg)
{
    _cache_item_t   *item = (_cache_item_t *)arg;
        
    ulm_free2(item->_list);
    ulm_free2(item->_listCounts);
    ulm_free2(item->_labels);
        
    ulm_free2(item);
}


inline unsigned int _hamming_distance(unsigned int node1, unsigned int node2)
{
    unsigned int    val;
        
    val = node1 ^ node2;            /* XOR */
    /* now count # of bits in val equal to 1. */
    return _hamming_weight(val);
}

inline unsigned int _hamming_weight(unsigned int node)
{
    unsigned int    cnt;
        
    /* count # of bits in node equal to 1. */
    cnt = node & 1;
    while ( node )
    {
        node = node >> 1;
        cnt += (node & 1);
    }
    return cnt;
}


inline int _link_idx(unsigned int node1, unsigned int node2)
{
    /*  PRE: _hamming_distance(node1,  node2) == 1 */
    /*  identifies the bit in which node1 and node2 differ. */
        
    unsigned int    val;
    int                             idx;
        
    val = node1 ^ node2;            /* XOR */
    idx = -1;
    while ( val )
    {
        val =  val >> 1;
        idx++;
    }
        
    /* ASSERT: 2^idx == node1  XOR node2 */
        
    return idx;
}


#ifndef SUBCUBE_SZ
#define SUBCUBE_SZ              3
#endif


HypercubeNode::HypercubeNode(unsigned int label, unsigned int numberOfNodes)
    :  CTNode(label, numberOfNodes), subcubesz_m(SUBCUBE_SZ), 
       hsz_m(ulm_log2(numberOfNodes))
{
    ntype_m = kHypercube;
        
    ghost_m = label ^ ( 1 << hsz_m);
    /* hsize is dimension of hypercube. */
    scatterCache_m.setFreeFunction(_free_cache_item);
    broadcastCache_m.setFreeFunction(_free_cache_item);
}

/*
 *
 *                      Class methods
 *
 */
         


unsigned int HypercubeNode::initialControlData(unsigned int hsize)
{
    static unsigned int             _init_ctrl = (1 << ulm_log2(hsize)) - 1;
        
    /* hsize is number of nodes. */
    return _init_ctrl;
}

unsigned int *HypercubeNode::subcube(unsigned int numNodes, unsigned int subdim, 
                                     unsigned int node, unsigned int index, unsigned int *cnt)
{
    unsigned int    hdim, nsnodes, chk, j, start, offset;
    unsigned int    *subcube, mx, *ptr;
        
    /*
      If N is the number of nodes, then the nodes are labeled from 0 to N-1.
      The subcube index, j, is from 0,..., 2^(n-r) - 1, so that the the translation
      is 2^r * j.
    */
    hdim = ulm_log2(numNodes);
    subcube = NULL;
    *cnt = 0;
    nsnodes = (1 << subdim);
    start = (1 << (hdim - subdim)) - 1;
    start = ( index < start ) ? index : start;
    mx = (1 << hdim) - (1 << subdim);
        
    /* check if we should consider this subcube. */
    chk = ((start << subdim) ^ node) & mx;
    if ( (numNodes-1) < chk )
        return NULL;
                
    if ( NULL == (subcube = (unsigned int *)ulm_malloc(sizeof(unsigned int)*nsnodes)) )
    {
        return NULL;
    }
    offset = (start << subdim);
    ptr = subcube;
    for ( j = 0; j < nsnodes; j++ )
        if ( ((j + offset) ^ node) < numNodes )
        {
            *(ptr++) = (j + offset) ^ node;                 
            (*cnt)++;
        }

    return subcube;
}



/*
 *
 *                      Instance methods
 *
 */
         
        
char *HypercubeNode::initialControlData(unsigned int *ctrlSize)
{
    unsigned int    *ctl;
        
    *ctrlSize = 0;
    if ( (ctl = (unsigned int *)ulm_malloc(sizeof(unsigned int))) )
    {
        *ctl = (1 << hsz_m) - 1;
        *ctrlSize = sizeof(unsigned int);
        return (char *)ctl;             /* sets the lower hsz_m bits to 1. */
    }
    else
        return NULL;
}


/*
  void HypercubeNode::addNeighbor(unsigned int label, void *userInfo)
  {
  HypercubeNode           *node;
        
  node = new HypercubeNode(label, hsz_m);
  node->setUserInfo(userInfo);
  CTNode::addNeighbor(node);
  }
*/

bool HypercubeNode::isANeighbor(unsigned int label)
{
    /*
      We want to allow for ghost nodes so that the network
      is a complete hypercube even if we don't have number of nodes = 2^n.
    */
    unsigned int    dist;
    bool                    isa;
        
    return (_hamming_distance(label_m, label) == 1);
        
    dist = _hamming_distance(label_m, label);
    isa = ( 1 == dist);
    if ( false == isa )
    {
        /*
          If we don't have a complete hypercube, then this node is a neighbor
          if its ghost label would represent a neighbor node.
          (Don't make a neighbor of ourselves).
        */
        if ( (numNodes_m < ghost_m) && dist )
            isa = (_hamming_distance(ghost_m, label) == 1);
    }
    return isa;
}


unsigned int HypercubeNode::labelForLink(unsigned int link)
{
    return link ^ label_m;
}
        

unsigned int HypercubeNode::nextNeighborLabelToNode(unsigned int toLabel)
{
    unsigned int    link, reladdr;
        
    /* compute the link to follow from current node when routing a pt-to-pt msg. */
    reladdr = toLabel ^ label_m;
    link = 1 << hsz_m;
    while ( link )
    {
        if ( link & reladdr )
            if ( neighbor(link ^ label_m) )
                break;
        link = link >> 1;
    }
        
    return link ^ label_m;
}


char *HypercubeNode::controlForLinking(char *controlData, unsigned int link, unsigned int *sz)
{
    /*
      PRE:    Neighbor node along link exists.
    */
        
    unsigned int    ctl = *(unsigned int *)controlData, *newControl;
    unsigned int    lnk;
        
    /* ASSERT: controlData is pointer to unsigned int. bit j is link j. */
    /*
      This method should be used in conjuction with broadcasting.  It assumes that the
      current node is participating in a bcast and controlData has been passed to it
      from a parent node in the spanning tree.  The new control structure is the control
      that will be passed to neighbor node along specified link and is based on bcast
      algorithm by Katseff.
    */
    *sz = 0;
    if ( (newControl = (unsigned int *)ulm_malloc(sizeof(unsigned int))) )
    {               
        /*
          Scan the links j such that j >= link.   bit j is 1 if
          bit j in ctl is 1 AND neighbor along link j does not exist.
        */
                
        *newControl = 0;
        // Now process bits i = 0 to j-1. bit i of newControl = bit i of ctl
        lnk = link >> 1;
        while ( lnk )
        {
            *newControl |= (lnk & ctl);
            lnk >>= 1;
        }
        *sz = sizeof(unsigned int);
    }
        
    return (char *)newControl;
}
        



unsigned int *HypercubeNode::broadcastList(char *controlData, unsigned int *cnt)
{
    /*
      POST:   returns an array of neighbor links along which the msg should be forwarded.
      This is for the initial bcast call by the source node.
    */
    unsigned int    *list, ncnt, lnk, idx;
    unsigned int    ctl;
    _cache_item_t   *item;
        
    /* ASSERT: controlData is pointer to unsigned int. bit j is link j. 
       The link value that is returned is 2^(j-1) if link is numbered from 1 to n.
    */
    *cnt = ncnt = 0;
        
    if ( !controlData ) return NULL;
        
    ctl = *(unsigned int *)controlData;
    /* first check cache to see if list already exists. 
       Cache is a hash table whose key is the control struct and value is a struct
       with fields: scatter list, labels
    */
    if ( (item = (_cache_item_t *)broadcastCache_m.valueForKey(ctl)) )
    {
        list = item->_labels;
        *cnt = item->cnt_m;
        return list;
    }

    list = (unsigned int *)ulm_malloc(sizeof(unsigned int)*HCUBE_MAX_SZ);
    if ( list )
    {
        /* loop scans from highest link number to lowest. We want to schedule
           broadcasting by size of spanning subtree. */
        lnk = 1 << (HCUBE_MAX_SZ - 1);
        while ( lnk )
        {
            if ( (idx = lnk & ctl) )
            {
                // check that neighbor exists
                if ( NULL != CTNode::neighbor(label_m ^ idx) )
                {
                    list[ncnt++] = lnk;
                }
            }
            lnk = lnk >> 1;
        }
                
        /* store list in cache. */
        if ( (item = (_cache_item_t *)ulm_malloc(sizeof(_cache_item_t))) )
        {
            item->_list = NULL;
            item->_listCounts = NULL;
            item->_labels = list;
            item->cnt_m = ncnt;
                        
            broadcastCache_m.setValueForKey(item, ctl);
        }
    }
    *cnt = ncnt;
        
    return list;
}


unsigned int *HypercubeNode::children(unsigned int source, unsigned int *cnt)
{
    /*
      ASSERT: - link from node u to node v exists iff link from node v to node u exists.
    */
    unsigned int    i, idx, *children, reladdr, label;
    bool                    exists;
        
    // We loop through all possible child nodes and check if current node
    // would be a parent.  Current node is a parent if it is in the path for
    // a pt-to-pt message to source.
    *cnt = 0;
    if ( NULL == (children = (unsigned int *)ulm_malloc(sizeof(unsigned int)*hsz_m)) )
        return NULL;
        
    if ( (unsigned int)(1 << (hsz_m-1)) == numNodes_m )
    {
        //We use the children() and parent() functions in the paper  by Johnsson and Ho:
        //Optimum Broadcasting and Personalized Communication in Hypercubes.
        // compute relative address of this node and source. Then find the first most significant bit, j,
        // such that bit j is 1.  Then children are label_m XOR (2^k), k = n-1, ..., j+1.
        reladdr = label_m ^ source;
        if ( 0 == reladdr )
            idx = 0;
        else
        {
            idx = hsz_m - 1;
            while ( 0 == (reladdr & (1 << (hsz_m-1))) )
            {
                reladdr <<= 1;
                idx--;
            }
            idx++;
        }               
        for ( i = hsz_m - 1; i >= idx; i-- )
            children[(*cnt)++] = label_m ^  (1 << i);
    }
    else
    {
        //Logic for incomplete hypercube.
                
        for ( i = 0; i < hsz_m; i++ )
        {
            label = label_m ^ (1 << i);
            if ( neighbor(label) )
            {
                // NOTE: This only works for non-faulty networks.  Need to add extra
                // logic for faulty links.
                // Current logic attempts to do the same logic as nextNeighborLabelToNode().
                                
                // link exists. Next, compute relative address from neighbor to source.
                // Then find most significant bit, j, that is 1 and link j exists from
                // neighbor (for non-faulty network, link j exists if neighbor XOR (2^j) < N).
                // If so and j == i, then neighbor is a child.
                reladdr = label ^  source;
                idx = 1 << (hsz_m - 1);
                exists = false;
                while ( idx && !exists )
                {
                    if ( ((label ^ idx) < numNodes_m) && (reladdr & idx) )
                        exists = true;
                    else
                    {
                        idx >>= 1;
                    }
                }
                if ( (unsigned int)(1 << i) == idx )
                    children[(*cnt)++] = label;
            }  
        }
    }
        
        
    return children;
}


bool HypercubeNode::parent(unsigned int source, unsigned int *parent)
{
    /*
      ASSERT: - link from node u to node v exists iff link from node v to node u exists.
      - Hypercube is a complete hypercube, using ghost nodes if necessary.
    */
    unsigned int            idx, reladdr;
    bool                            exists, success;
        
    success = true;
    if ( source == label_m )
        return false;
    else
    {
        exists = false;
        reladdr = label_m ^ source;
        idx = 1 << (hsz_m - 1);
        while ( !exists && idx )
        {
            if ( reladdr & idx )
            {
                if ( neighbor(label_m ^ idx) )
                {
                    exists = true;
                    *parent = label_m ^ idx;
                }
            }
            idx >>= 1;
        }
                
        success = exists;
                
        /*
        //We use the children() and parent() functions in the paper  by Johnsson and Ho:
        //Optimum Broadcasting and Personalized Communication in Hypercubes.
        // compute relative address of this node and source. Then find the first most significant bit, j,
        // such that bit j is 1.  Then children are label_m XOR (2^k), k = n-1, ..., j+1.
        reladdr = label_m ^ source;
        idx = hsz_m - 1;
        while ( 0 == (reladdr & (1 << (hsz_m-1))) )
        {
        reladdr <<= 1;
        idx--;
        }
        *parent = label_m ^ (1 << idx);
        */
    }
        
    return success;
}


unsigned int **HypercubeNode::scatterList(unsigned int **links, unsigned int **counts, unsigned int *arrayCnt)
{
    /*
      POST:   returns dynamic array of node links that form
      a list of which neighbors the node should forward a
      scatter/scatterv message to.
      Returns corresponding list of links so that links[i] = link along which to 
      forward to nodes in scatterList[i].
    */

    unsigned int    **scatterList, *listCounts, *ptr, mx,  chk;
    unsigned int    i, j, sz, start, nsubcubes, nsnodes, offset, idx;
    _scatter_t              *cached;

    /* check cache first. */
    if ( cachedScatterList_m )
    {
        cached = (_scatter_t *)cachedScatterList_m;
        *links = cached->_links;
        *counts = cached->_listCounts;
        *arrayCnt = cached->cnt_m;
                
        return cached->_list;
    }
        
    /* generate all subcubes for scattering.  We want to partition the hypercube so that if
       it is large, we don't end up sending a single huge message.  Instead, send in chunks.
    */
    *links = *counts = NULL;
    *arrayCnt = 0;
        
    /* Order scatter list by distance of subcube from this node (descending). 
       Create subcube that's furthest away.  Construct by creating subcube first based on
       node 0, then translate to current node.
    */
    nsnodes = (1 << subcubesz_m);
    nsubcubes = (numNodes_m / nsnodes) + ((numNodes_m & (nsnodes-1)) > 0);
    sz = sizeof(unsigned int *) * nsubcubes;
    scatterList = (unsigned int **)ulm_malloc(sz);
    if ( !scatterList )
        return NULL;
    listCounts = (unsigned int *)ulm_malloc(sz);
    if ( !listCounts )
    {
        ulm_free2(scatterList);
        return NULL;
    }
        
    *arrayCnt = nsubcubes;
    bzero(scatterList, sz);
    bzero(listCounts, sz);
        
    /*
      If N is the number of nodes, then the nodes are labeled from 0 to N-1.
      The subcube index, j, is from 0,..., 2^(n-r) - 1, so that the the translation
      is 2^r * j.
    */
    start = (1 << (hsz_m - subcubesz_m)) - 1;
    mx = (1 << hsz_m) - (1 << subcubesz_m);
    idx = 0;
        
    for ( i = start; (int)i >= 0; i-- )
    {
        /* check if we should consider this subcube. */
        chk = ((i << subcubesz_m) ^ label_m) & mx;
        if ( (numNodes_m-1) < chk )
            continue;
                        
        if ( NULL == (scatterList[idx] = (unsigned int *)ulm_malloc(sizeof(unsigned int)*nsnodes)) )
        {
            free_double_iarray((int **)scatterList, *arrayCnt);
            ulm_free2(listCounts);
            return NULL;
        }
        offset = (i << subcubesz_m);
        listCounts[idx] = 0;
        ptr = scatterList[idx];
        for ( j = 0; j < nsnodes; j++ )
            if ( ((j + offset) ^ label_m) < numNodes_m )
            {
                *(ptr++) = (j + offset) ^ label_m;                      
                listCounts[idx]++;
            }
                
        idx++;
    }
        
    /* cache scatter list. */
    if ( (cached = (_scatter_t *)ulm_malloc(sizeof(_scatter_t))) )
    {
        bzero(cached, sizeof(_scatter_t));
        cached->_listCounts = listCounts;
        cached->cnt_m = *arrayCnt;
        cached->_list = scatterList;
        cachedScatterList_m = cached;
    }
        
    *counts = listCounts;
        
    return scatterList;
}       



unsigned int **HypercubeNode::scatterList(char *controlData, unsigned int *labels, unsigned int labelCnt,
                                          unsigned int **links, unsigned int **counts, unsigned int *arrayCnt)
{
    /*
      PRE:    labels is an ordered list.
      The scatter operation is for the entire hypercube
    */
    /* ASSERT: controlData is pointer to unsigned int. bit j is link j. */
        
    /* this algorithm uses the same spanning subtree algorithm as in broadcast,
       but we want to order the links so that the link's subtree are in descending
       order in terms of the size of the subtrees.
    */
        
    /* IMPORTANT: the broadcastList() algorithm scans the link numbers in
       descending order, which is our desired scatter order.
    */
    unsigned int    *list, **scatterList, *listCounts, ncnt, idx, treesz;
    unsigned int    ctl = *(unsigned int *)controlData, chk, *newctrl;
    unsigned int    rlabel, msk, csz;
    unsigned int    i, j;
    _cache_item_t           *item;
        
    /* ASSERT: controlData is pointer to unsigned int. bit j is link j. */
    *arrayCnt = ncnt = 0;
    scatterList = NULL;
    *counts = NULL;
    *links = NULL;
        
    if ( !ctl ) return NULL;
        
    /* first check cache to see if list already exists. 
       Cache is a hash table whose key is the control struct and value is a struct
       with fields: scatter list, labels.
       NOTE: cache assumes that the scatter operation is for entire hypercube.
    */
    *links = list = broadcastList(controlData, &ncnt);
    if ( (item = (_cache_item_t *)scatterCache_m.valueForKey(ctl)) )
    {
        *counts = item->_listCounts;
        *arrayCnt = item->cnt_m;
        return item->_list;
    }

    if ( 0 == ncnt )
        return NULL;
    
    scatterList =  (unsigned int **)ulm_malloc(sizeof(unsigned int  *)*ncnt);
    listCounts =  (unsigned int *)ulm_malloc(sizeof(unsigned int)*ncnt);
    if ( !scatterList || !listCounts )
    {
        ulm_free2(scatterList);
        ulm_free2(listCounts);
        return NULL;
    }
        
    bzero(listCounts, sizeof(unsigned int)*ncnt);
    bzero(scatterList, sizeof(unsigned int  *)*ncnt);
        
    /* for each link, compute v AND ~(control for v) */
    for ( i = 0; i < ncnt; i++ )
    {
        /* rlabel is the label of the neighboring node along link links[i]. */
        rlabel = label_m ^ list[i];
        newctrl = (unsigned int *)controlDataForSending(controlData, list[i], &csz);
        msk = (~(*newctrl)) &  ( (1 << hsz_m) - 1) ;
        chk = rlabel & msk;
        ulm_free2(newctrl);
                
        // determine max size of subtree with root rlabel. 
        // This will be 2^(hsz_m - Hamming weight of chk)
        treesz = 1 << (hsz_m -  _hamming_weight(chk));
        if ( NULL == (scatterList[i] = (unsigned int *)ulm_malloc(sizeof(unsigned int)*treesz)) )
        {
            free_double_iarray((int**)scatterList, i);
            ulm_free2(listCounts);
            return NULL;
        }
                
        // Now go through labels array and check if the label belongs in subtree of rlabel.
        idx = 0;
        for ( j = 0; j < labelCnt; j++ )
        {
            // skip root label
            if ( ((labels[j] & msk) == chk) )
            {
                scatterList[i][idx++] = labels[j];
                listCounts[i]++;
            }
        }
    }
        
        
                
    /* store list in cache. */
    if ( (item = (_cache_item_t *)ulm_malloc(sizeof(_cache_item_t))) )
    {
        item->_list = scatterList;
        item->_listCounts = listCounts;
        item->cnt_m = ncnt;
                
        scatterCache_m.setValueForKey(item, ctl);
    }               
    *counts = listCounts;           
    *arrayCnt = ncnt;
        
    return scatterList;
}




char *HypercubeNode::controlDataForSending(char *controlData, unsigned int link, unsigned int *sz)
{
    /*
      PRE:    Neighbor node along link exists.
    */
        
    unsigned int    ctl = *(unsigned int *)controlData, *newControl;
    unsigned int    lnk, idx;
        
    /* ASSERT: controlData is pointer to unsigned int. bit j is link j. */
    /*
      This method should be used in conjuction with broadcasting.  It assumes that the
      current node is participating in a bcast and controlData has been passed to it
      from a parent node in the spanning tree.  The new control structure is the control
      that will be passed to neighbor node along specified link and is based on bcast
      algorithm by Katseff.
    */
    *sz = 0;
    if ( (newControl = (unsigned int *)ulm_malloc(sizeof(unsigned int))) )
    {               
        /*
          Scan the links j such that j >= link.   bit j is 1 if
          bit j in ctl is 1 AND neighbor along link j does not exist.
        */
                
        *newControl = 0;
                
        lnk = 1 << (hsz_m - 1);
        while ( lnk >= link )
        {
            // check that bit lnk is 1 in ctl
            if ( (idx = lnk & ctl) )
            {
                // If neighbor does not exists, then turn on bit
                if ( NULL == CTNode::neighbor(label_m ^ idx) )
                {
                    *newControl |= lnk;
                }
            }
            lnk = lnk >> 1;
        }
                
        // Now process bits i = 0 to j-1. bit i of newControl = bit i of ctl
        while ( lnk )
        {
            *newControl |= (lnk & ctl);
            lnk >>= 1;
        }
        *sz = sizeof(unsigned int);
    }
        
    return (char *)newControl;
}
