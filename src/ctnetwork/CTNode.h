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
 *  CTNode.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Tue Jan 07 2003.
 */

#ifndef _NETWORK_NODE_H_
#define _NETWORK_NODE_H_

#include "internal/MPIIncludes.h"
#include "util/HashTable.h"


//typedef CTNode                *(*node_create_fn)(unsigned int label, unsigned int numberOfNodes);


class CTNode
{
    /*
     *      instance variables
     */
         
protected:
        
    unsigned int    label_m;                        /* non-negative value. */
    unsigned int    ntype_m;                        /* hypercube, etc. */
    unsigned int    numNodes_m;             /* total number of nodes (makes it more efficient for routing) */
    void                    *userInfo_m;
    HashTable               neighbors_m;
    HashTable               scatterCache_m;
    HashTable               broadcastCache_m;
    void                    *cachedScatterList_m;

public:
    enum
        {
            kHypercube = 1
        };
        
        

    /*
     *      instance methods
     */
         

public:
        
    static CTNode *createNode(unsigned int type, unsigned int label, unsigned int numberOfNodes);
        
    CTNode(unsigned int label, unsigned int numberOfNodes);
    virtual ~CTNode();
        
    /*
     *      topology mgmt methods
     */
         
    virtual void updateWithFailedNode(unsigned int nodeLabel) {};
        
    /*
     *      neighbor mgmt methods
     */
         
    void setNeighborDeleteMethod(void (*del_fn)(void*));
        
    virtual void *neighbor(unsigned int label);
    /*
      POST:   returns CTChannel instance with specified label
      that is neighbor of node.
    */
        
    virtual void addNeighbor(unsigned int label, void *neighbor);
    /*
      PRE:    label represents the label for a valid neighbor.
      POST:   Adds a link to a neighbor with specified label.
    */
        
        
    virtual bool isANeighbor(unsigned int label) = 0;
    /*
      POST:   returns true if a node with specified label is a label for
      a neighbor node.  This does not imply that the neighbor() method
      would return non-NULL; it only verifies that the label is a label
      for a neighbor.
    */
        
    virtual unsigned int labelForLink(unsigned int link) = 0;
    /*
      POST:   returns the label for the neighbor connected by link.
    */
        
    /*
     *      routing methods
     */
    virtual char *initialControlData(unsigned int *ctrlSize) = 0;

    virtual unsigned int nextNeighborLabelToNode(unsigned int toLabel) = 0;
    /*
      POST:   returns the neighbor label to use to forward a message to destination.
    */
        
    virtual char *controlForLinking(char *controlData, unsigned int link, unsigned int *sz) = 0;

    virtual unsigned int *broadcastList(char *controlData, unsigned int *cnt) = 0;
    /*
      PRE:    each node has a defined order to its edges. So link j
      is the edge connecting node x to node y[j], 1 <= j <= N, where N is
      the maximum number of edges.
      control is a topology dependent routing control structure.
      POST:   returns dynamic array of node links that form
      a list of which neighbors the node should forward a
      broadcast message to.
    */

    virtual unsigned int *children(unsigned int source, unsigned int *cnt) = 0;
    /*
      POST:   Returns array of labels of nodes that are children of current node within
      a spanning tree whose root is source.  User is responsible for freeing array.
    */
        
        
    virtual bool parent(unsigned int source, unsigned int *parent) = 0;
    /*
      POST:   if node has a parent, then returns true and the parent label.
    */
        
    virtual unsigned int **scatterList(unsigned int **links, unsigned int **counts, unsigned int *arrayCnt) = 0;
    /*
      POST:   returns dynamic array of node links that form
      a list of which neighbors the node should forward a
      scatter/scatterv message to.
      Returns corresponding list of links so that links[i] = link along which to 
      forward to nodes in scatterList[i].
    */
        
        
    virtual unsigned int **scatterList(char *controlData, unsigned int *labels, unsigned int labelCnt,
                                       unsigned int **links, unsigned int **counts, unsigned int *arrayCnt) = 0;
    /*
      PRE:    each node has a defined order to its edges. So link j
      is the edge connecting node x to node y[j].
      control is a topology dependent routing control structure.
      labels[i] < labels[i+1] (ascending order)
      POST:   returns dynamic array of node links that form
      a list of which neighbors the node should forward a
      scatter/scatterv message to.
      Returns corresponding list of links so that links[i] = link along which to 
      forward to nodes in scatterList[i].
    */

    virtual char *controlDataForSending(char *controlData, unsigned int link, unsigned int *sz) = 0;
    /*
      PRE:    The network has ordered the edges eminating from each node.  link identifies
      a specific link connecting two nodes.
      POST:   returns new control data for sending msg to node along specified link.  This
      is only meaningful with broadcasting.  User is responsible for freeing memory.
    */
         
    /*
     *      Accessor methods
     */
         
    unsigned int label() {return label_m;}
    void setLabel(unsigned int label) {label_m = label;}

    unsigned int numberOfNodes() {return numNodes_m;}
        
    unsigned int type() {return ntype_m;}
        
    void *userInfo() {return userInfo_m;}
    void setUserInfo(void *info) {userInfo_m = info;}
        
    bool hasNeighbors() {return (neighbors_m.count() > 0);}
        
};


class HypercubeNode : public CTNode
{
    /*
     *      instance variables
     */

private:
    unsigned int    ghost_m;                        /* label of ghost node for incomplete hypercube. */
    unsigned int    subcubesz_m;
    unsigned int    hsz_m;                  /* hsz_m = log2(# nodes in network) */
protected:
    void                    *faults_m;

    /*
     *      instance methods
     */
public:
    HypercubeNode(unsigned int label, unsigned int numberOfNodes);
        
    /*
     *
     *                      Class methods
     *
     */
         
    static unsigned int initialControlData(unsigned int hsize);
    /* hsize is number of nodes. */
        
    static unsigned int *subcube(unsigned int numNodes, unsigned int subdim, 
                                 unsigned int node, unsigned int index, unsigned int *cnt);
        
    /*
     *
     *                      Instance methods
     *
     */
         
        
    char *initialControlData(unsigned int *ctrlSize);
    /*
      PRE:    log2(number of nodes) == hsz_m
      POST:   returns pointer to unsigned int.  Caller is responsible for
      freeing memory.
    */
                
    bool isANeighbor(unsigned int label);
        
    unsigned int labelForLink(unsigned int link);
    /*
      POST:   returns the label for the neighbor connected by link.
    */
        
    unsigned int *pathToNode(unsigned int toLabel);
        
        
    /*
     *
     *      Accessor methods.
     *
     */
         
    unsigned int subcubeSize() {return subcubesz_m;}
    void setSubcubeSize(unsigned int sz) {if ( sz <= hsz_m) subcubesz_m = sz;}
        
    /*
     *
     *      CTNode virtual method implementation.
     *
     */
         
    unsigned int nextNeighborLabelToNode(unsigned int toLabel);
    /*
      POST:   returns the link to use to forward a message to destination.
    */

    char *controlForLinking(char *controlData, unsigned int link, unsigned int *sz);
        
    unsigned int *broadcastList(char *controlData, unsigned int *cnt);
    /*
      PRE:    controlData should be a pointer to an unsigned int.
      NOTE: This limits the size of the hypercube to 2^32 nodes.
      POST:   returns an array of links for neighbors that should receive
      the broadcast message based on the passed control data.
      Returns number of items in array in cnt.
    */

    unsigned int *children(unsigned int source, unsigned int *cnt);
    /*
      POST:   Returns array of labels of nodes that are children of current node within
      a spanning tree whose root is source.  User is responsible for freeing array.
      Returns NULL on error.
    */


    bool parent(unsigned int source, unsigned int *parent);

    unsigned int **scatterList(unsigned int **links, unsigned int **counts, unsigned int *arrayCnt);
    /*
      POST:   returns dynamic array of node links that form
      a list of which neighbors the node should forward a
      scatter/scatterv message to.
      Returns corresponding list of links so that links[i] = link along which to 
      forward to nodes in scatterList[i].
    */
        
    unsigned int **scatterList(char *controlData, unsigned int *labels, unsigned int labelCnt,
                               unsigned int **links, unsigned int **counts, unsigned int *arrayCnt);


    char *controlDataForSending(char *controlData, unsigned int link, unsigned int *sz);
    /*
      PRE:    neighbor connected via link exists.
      POST:   returns pointer to unsigned int.  Caller is responsible for
      freeing memory.
    */
        
};

#endif

