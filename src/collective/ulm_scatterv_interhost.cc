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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ulm/errors.h"
#include "internal/system.h"
#include "internal/collective.h"
#include "ulm/types.h"
#include "queue/Communicator.h"
#include "queue/globals.h"

/*
 * This routine is used to scatter data from the root node on the root
 *   root host (does not need to be the root of the scatter) to each other
 *   host root in the communicator.  Data is sent via intermediate hosts
 *   to produce logrithmic scaling.
 */
int ulm_scatterv_interhost(int comm, void *dataBuffer,
                           ulm_tree_t *treeData, size_t *dataPerHost, int root, int tag)
{
    Communicator *commPtr=(Communicator *) communicators[comm];
    int returnValue = ULM_SUCCESS;
    void *buffPtr[SCATTER_TREE_ORDER-1];
    size_t size[SCATTER_TREE_ORDER-1];
    int destinationProc[SCATTER_TREE_ORDER-1];
    ULMType_t *dtype[SCATTER_TREE_ORDER-1];
    int tags[SCATTER_TREE_ORDER-1];
    int comms[SCATTER_TREE_ORDER-1];
    ULMStatus_t status[SCATTER_TREE_ORDER-1];

    int myHostRank = commPtr->localGroup->hostIndexInGroup;

    /* figure out my node rank within the tree */
    Group *group=commPtr->localGroup;
    int rootHost=group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];
    int myTreeNodeID=myHostRank-rootHost;
    /* wrap around */
    if( myTreeNodeID < 0 )
        myTreeNodeID+=group->numberOfHostsInGroup;

    size_t myData=dataPerHost[myHostRank];

    /* receive data, if need be */
    if( treeData->nUpNodes[myTreeNodeID] == 1 ) {
        int srcHost=treeData->upNodes[myTreeNodeID];
        srcHost+=rootHost;
        if( srcHost >=treeData->nNodes )
            srcHost-=treeData->nNodes;
        int srcProc=group->groupHostData[srcHost].groupProcIDOnHost[0];
        /* amount of data to recv */
        size_t dataToRecv=myData;
        for( int h=1 ; h < group->lenScatterDataNodeList[myTreeNodeID] ; h++ ) {
            int host=group->scatterDataNodeList[myTreeNodeID][h];
            host+=rootHost;
            if( host >=group->numberOfHostsInGroup )
                host-=group->numberOfHostsInGroup;
            dataToRecv+=dataPerHost[host];
        }
        /* data destined for root host */
        returnValue=ulm_recv(dataBuffer,dataToRecv,(ULMType_t *) MPI_BYTE,
                             srcProc,tag,commPtr->contextID,status);
        if( returnValue != ULM_SUCCESS )
            return returnValue;
    }

    /* send data on */

    /* return, if no one to send to */
    int nDownNodes=treeData->nDownNodes[myTreeNodeID];
    if( nDownNodes == 0 )
        return returnValue;

    /* loop over full levels - sending SCATTER_TREE_ORDER-1 messages
     *   at a time
     */

    /* pointer to next memory region where data will be sent from */
    void *ptrIntoDataBuffer=(void *) ( (char *)dataBuffer +myData );
    int numsent=0;
    int numSent=0;
    /* loop until all messages have been sent */
    while( numSent < treeData->nDownNodes[myTreeNodeID] ) {
        /* loop over all child nodes at a given level */
        int maxSendPerIter=SCATTER_TREE_ORDER-1;
        int nSetUp=0;
        /* setup up to maxSendPerIter at a time */
        while ( (nSetUp < maxSendPerIter) && ( numsent < treeData->nDownNodes[myTreeNodeID] ) ) {
            /* compute destination process */
            int destNode=treeData->downNodes[myTreeNodeID][numsent];
            int destHost=destNode+rootHost;
            if( destHost >=treeData->nNodes )
                destHost-=treeData->nNodes;
            destinationProc[nSetUp]=group->groupHostData[destHost].groupProcIDOnHost[0];
            /* compute the amount of data to be send */
            size_t dataToSend=0;
            for( int h=0 ; h < group->lenScatterDataNodeList[destNode] ; h++ ) {
                int host=group->scatterDataNodeList[destNode][h];
                host=host+rootHost;
                if( host >= treeData->nNodes )
                    host-=treeData->nNodes;
                dataToSend+=dataPerHost[host];
            }  /* end h loop */
            size[nSetUp]=dataToSend;
            /* pointer to data */
            buffPtr[nSetUp]=ptrIntoDataBuffer;
            /* data type */
            dtype[nSetUp]=(ULMType_t *) MPI_BYTE;
            /* tag */
            tags[nSetUp]=tag;
            /* communicator */
            comms[nSetUp]=comm;
            ptrIntoDataBuffer=(void *)( (char *)ptrIntoDataBuffer + dataToSend );
            numsent++;
            nSetUp++;
        }
        /* send data */
        returnValue=ulm_send_vec(buffPtr, size, dtype, destinationProc, tags, comms,
                                 nSetUp, status);
        if( returnValue != ULM_SUCCESS )
            return returnValue;
        numSent+=nSetUp;
    }

    return returnValue;
}
