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
 *  CTServer.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 *
 */

#ifndef _CT_SERVER_H_
#define _CT_SERVER_H_

#include "ctnetwork/CTController.h"
#include "ctnetwork/CTClient.h"
#include "ctnetwork/CTMessage.h"
#include "ctnetwork/CTNode.h"
#include "threads/Runnable.h"
#include "os/atomic.h"
#include "util/linked_list.h"

#define CT_SVR_MAX_COLL         8
#define CT_SVR_COLL_START       kAllgather
#define COLL_IDX(x)                     x - CTServer::kAllgather

class CTServer : public CTController
{
    /*
     * Instance Variables
     */
private:
    CTChannelServer     *chserver_m;
    volatile bool       hasLinkedNet_m;
    bool                isStarted_m;
    volatile bool       performingChecks_m;
    bool                collOK_m[CT_SVR_MAX_COLL];
    lockStructure_t     lock_m;
    lockStructure_t     coll_lock_m;
    lockStructure_t     scanlock_m;
    unsigned int        netxtID_m;
    unsigned int        clientCnt_m[CT_SVR_MAX_COLL];
    unsigned int        childCnt_m[CT_SVR_MAX_COLL];
    char                *buffer_m[CT_SVR_MAX_COLL];
        
protected:
    CTNode              *node_m;
    link_t              *clients_m;
    link_t              *collClients_m[CT_SVR_MAX_COLL];                /* used for tracking which clients to send collective response msg. */
        
public:
        
    /*
      NETWORK messages.
    */
    enum
        {
            kUnknownCmd = 0,
            kLinkNetwork,                               /* perform network link up (for not-so-large networks). */
            kLinkData,                                  /* msg contains link info. */
            kGetServerInfo,                             /* meant for initial handshake with CTClient. (Synchronous) */
            kVerifyNode,                                /* determine if a node is alive. */
                
            /* collectives (NOTE: keep collectives together) */
            kAllgather,                                 /* cmd for allgather collective. */
            kAllgatherv,                                /* cmd for allgather collective. */
            kSynchronize                                /* cmd for synchronization. */
        };

    /*
     * Instance Methods
     */
private:

    /*
     *          Channel mgmt
     */
    bool checkClientStatus(CTChannelStatus status, link_t       *client);
        
    /*
     *          Network admin tasks
     */
    void connectToNeighbor(unsigned int neighborLabel, const char *connectInfo);
        
    void getServerInfo(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl);
        
    bool linkNetwork(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl);
        
    bool allgather(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl);
        
    bool synchronize(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl);
        
    /*
     *          Message routing
     */
    void broadcastPackedMessage(char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl);
    void handleMessage(CTChannel *chnl, char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl);
    void handleNetMsg(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl);
    void routeMessage(CTChannel *chnl, char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl);
    void scattervPackedMessage(char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl);
    bool scatterv(unsigned int sourceLabel, int msgType, unsigned int labelCnt, unsigned int *labels,
                  unsigned int *datalen, const char **data, char *ctrl, unsigned int ctrlsz);
        
protected:
    void performChecks(void *arg);
    /*
      This method is normally run in a separate thread.  
      Server performs:
      1) Scans client connections for data
      2) Check each neighbor to see if any failed
    */
        
    void acceptConnections(void *arg);
    /*
      This method is normally run in a separate thread.
      Waits for client connections.
    */
        
        
public:
    CTServer(CTChannelServer *channel);
    virtual ~CTServer();


    static void CTServerInit();
        
        
    /* Helper functions for network admin. */
    static CTMessage *getServerInfoMessage();
    /*
      POST:     Creates the necessary message to send to a CTServer instance
      to retrieve the server's node info.  This is a synchronous process so the
      sender should expect to receive a return message.  The return message data should
      have the string format:
      Label=<node label>;Network=<Network type, e.g. hypercube>
    */
        
    static CTMessage *linkNetworkMessage(int networkType, int numNodes, unsigned int *nodeLabels, 
                                         const char **nodeConnectInfo);
    /*
      PRE:      controlData should be provided by a concrete CTNode class method.
      controlData should be in packed format and ctlDataSize should be the byte size
      of controlData.
      POST:     Creates the necessary message to send to a CTServer instance
      to initiate network linkup.
    */
        
        
        
        
    bool start();
    /*
      Creates threads to open channel for accepting connections and
      to check the neighbors' heartbeat.
    */
        
    void stop();
        

    /*
     * Messaging Operations
     */

    virtual CTChannelStatus broadcast(CTMessage *msg, unsigned int ctrlSize, char *control);
    virtual CTChannelStatus broadcast(CTMessage *msg);

    virtual CTChannelStatus sendMessage(CTMessage *msg);
    /*
      POST:     sends pt-to-pt message to destination set in  msg.
    */
        
        
    /*
     * Collective Operations
     */

    CTChannelStatus allgather(unsigned int numParticipants, unsigned int datalen, 
                              char *sendBuf, char *recvBuf);
    /*
      POST:     Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
      and participating CTClients. recvBuf +i*datalen is data from node i and length datalen.
    */

    CTChannelStatus allgatherv(unsigned int numParticipants, unsigned int sendlen, 
                               char *sendBuf, int *recvLens, char *recvBuf);
    /*
      POST:     Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
      and participating CTClients. recvBuf + recvOffsets[i] is data from node i and
      length recvOffsets[i] - recvOffsets[i-1].
    */

    virtual CTChannelStatus scatterv(int msgType, unsigned int *datalen, const char **data);
        
    virtual CTChannelStatus synchronize(unsigned int numParticipants);



    /*
     * Accessor Methods
     */
    CTChannel *channel() {return chserver_m->channel();}
        
    bool networkHasLinked() {return hasLinkedNet_m;}
        
    CTNode *node();
    void setNode(CTNode *node);
};

#endif
