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



#include "ulm/errors.h"
#include "internal/system.h"
#include "internal/collective.h"
#include "ulm/types.h"
#include "queue/Communicator.h"
#include "queue/globals.h"

/*
 * This routine is used to gather data from the root on each host
 *   in the communicator to the comm-root on the root host.
 */
int ulm_gather_interhost(int comm, void *SharedBuffer,
                         ulm_tree_t *treeData, size_t *dataToSendNow, int root, int tag)
{
    Communicator *commPtr=(Communicator *) communicators[comm];
    int i;
    int returnValue = ULM_SUCCESS;
    void *buffPtr[GATHER_TREE_ORDER-1];
    size_t size[GATHER_TREE_ORDER-1];
    int sourceProc[GATHER_TREE_ORDER-1];
    ULMType_t *dtype[GATHER_TREE_ORDER-1];
    int tags[GATHER_TREE_ORDER-1];
    int comms[GATHER_TREE_ORDER-1];
    ULMStatus_t status[GATHER_TREE_ORDER-1];

    int myHostRank = commPtr->localGroup->hostIndexInGroup;

    /* figure out my node rank within the tree */
    Group *group=commPtr->localGroup;
    int rootHost=group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];
    int myTreeNodeID=myHostRank-rootHost;
    /* wrap around */
    if( myTreeNodeID < 0 )
        myTreeNodeID+=group->numberOfHostsInGroup;

    /* pointer to next shared memory region where data will be recevied
     * into
     */
    void *sharedMemRecvPtr=(void *) ( (char *)SharedBuffer +
                                      dataToSendNow[myHostRank]);

    /*
     * first recv, from one level at a time
     */
    int nRecved=0;

    /* partial last level */
    int nInPartialLevel=treeData->nInPartialLevel[myTreeNodeID];
    int nDownNodes=treeData->nDownNodes[myTreeNodeID];
    if( nInPartialLevel > 0 ) {
        /* fill in the source process */
        int startIndex=nDownNodes-nInPartialLevel;
        for( i=startIndex ; i < nDownNodes; i++ ) {
            /* get host index in "shifted" host index list */
            int srcHost=treeData->downNodes[myTreeNodeID][i];

            /* get real host index */
            int sourceHost=srcHost+rootHost;
            if( sourceHost >=treeData->nNodes )
                sourceHost-=treeData->nNodes;
            sourceProc[i-startIndex]=group->groupHostData[sourceHost].groupProcIDOnHost[0];

            /* fill in the amount of data to recv */
            size_t bytesToRecv=0;
            for( int h=0; h < group->lenGatherDataNodeList[srcHost] ; h++ ) {
                int hst=group->gatherDataNodeList[srcHost][h]+rootHost;
                if( hst>=treeData->nNodes )
                    hst-=treeData->nNodes;
                bytesToRecv+=dataToSendNow[hst];
            }
            size[i-startIndex]=bytesToRecv;

            /* fill in destination buffer pointers */
            buffPtr[i-startIndex]=sharedMemRecvPtr;
            sharedMemRecvPtr=(void *)( (char *)sharedMemRecvPtr + size[i-startIndex]);

            /* fill in data type */
            dtype[i-startIndex]=(ULMType_t *) MPI_BYTE;

            /* fill in tag */
            tags[i-startIndex]=tag;

            /* fill in communicator index */
            comms[i-startIndex]=comm;

        }
        returnValue=ulm_recv_vec(buffPtr, size, dtype, sourceProc, tags, comms,
                                 nInPartialLevel, status);
        if( returnValue != ULM_SUCCESS )
            return returnValue;

        nRecved+=nInPartialLevel;
    }

    int currentLevel=treeData->nFullLevels-1;
    while ( nRecved <  treeData->nDownNodes[myTreeNodeID] ) {

        int startIndex=nDownNodes-(nRecved)-(GATHER_TREE_ORDER-1);
        for( i=startIndex ; i < startIndex+(GATHER_TREE_ORDER-1) ; i++ ) {
            /* get host index in "shifted" host index list */
            int srcHost=treeData->downNodes[myTreeNodeID][i];

            /* get real host index */
            int sourceHost=srcHost+rootHost;
            if( sourceHost >=treeData->nNodes )
                sourceHost-=treeData->nNodes;
            sourceProc[i-startIndex]=group->groupHostData[sourceHost].groupProcIDOnHost[0];

            /* fill in the amount of data to recv */
            size_t bytesToRecv=0;
            for( int h=0; h < group->lenGatherDataNodeList[srcHost] ; h++ ) {
                int hst=group->gatherDataNodeList[srcHost][h]+rootHost;
                if( hst>=treeData->nNodes )
                    hst-=treeData->nNodes;
                bytesToRecv+=dataToSendNow[hst];
            }
            size[i-startIndex]=bytesToRecv;

            /* fill in destination buffer pointers */
            buffPtr[i-startIndex]=sharedMemRecvPtr;
            sharedMemRecvPtr=(void *)( (char *)sharedMemRecvPtr + size[i-startIndex]);

            /* fill in data type */
            dtype[i-startIndex]=(ULMType_t *) MPI_BYTE;

            /* fill in tag */
            tags[i-startIndex]=tag;

            /* fill in communicator index */
            comms[i-startIndex]=comm;
        }

        returnValue=ulm_recv_vec(buffPtr, size, dtype, sourceProc, tags, comms,
                                 GATHER_TREE_ORDER-1, status);
        if( returnValue != ULM_SUCCESS )
            return returnValue;

        nRecved+=(GATHER_TREE_ORDER-1);
        currentLevel--;
    }

    /*
     * finally, send on current data
     */
    if( treeData->nUpNodes[myTreeNodeID] == 1 ) {
        int destHost=treeData->upNodes[myTreeNodeID];
        int destinationHost=destHost+rootHost;
        if( destinationHost >=treeData->nNodes )
            destinationHost-=treeData->nNodes;
        int destProc=group->groupHostData[destinationHost].groupProcIDOnHost[0];
        size_t bytesToSend=0;
        for( int h=0; h < group->lenGatherDataNodeList[myTreeNodeID] ; h++ ) {
            int hst=group->gatherDataNodeList[myTreeNodeID][h]+rootHost;
            if( hst>=treeData->nNodes )
                hst-=treeData->nNodes;
            bytesToSend+=dataToSendNow[hst];
        }
        returnValue=ulm_send(SharedBuffer,bytesToSend,(ULMType_t *) MPI_BYTE,
                             destProc,tag,commPtr->contextID,ULM_SEND_STANDARD);
    }


    return returnValue;
}
