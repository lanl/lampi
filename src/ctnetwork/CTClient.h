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
 *  AdminClient.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#ifndef _ADMIN_CLIENT_H_
#define _ADMIN_CLIENT_H_


#include "internal/MPIIncludes.h"
#include "ctnetwork/CTController.h"
#include "ctnetwork/CTNode.h"


class CTClient : public CTController
{
private:
    unsigned int    nodeLabel_m;
    unsigned int    nodeType_m;
    unsigned int    numNodes_m;
    CTNode                  *node_m;
        
private:
    void getServerInfo();
        
public:
    CTClient(CTChannel *channel);
        
    virtual CTChannelStatus sendMessage(CTMessage *msg, unsigned int ctrlSize, char *control);
        
    virtual CTChannelStatus broadcast(CTMessage *msg, unsigned int ctrlSize, char *control);

    CTChannelStatus allgather(unsigned int numParticipants, unsigned int datalen, 
                              char *sendBuf, char *recvBuf);
    /*
      PRE:    This implementation ignores sendBuf for CTClient.  Use this to gather data from
      network nodes.
      POST:   Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
      and participating CTClients. recvBuf +i*datalen is data from node i and length datalen.
    */


    CTChannelStatus allgatherv(unsigned int numParticipants, char *recvBuf);
    /*
      PRE:    recvBuf should be large enough to contain the received data.  It is assumed that
      the caller knows in advance the total size and the indvidual node data sizes.
      POST:   Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
      and participating CTClients.
    */


    virtual CTChannelStatus scatterv(int msgType, unsigned int *datalen, const char **data);
    /*
      PRE:    The number of items in array should equal the number of nodes in the network.
      Client is connected to a network node server and nodeLabel_m should be the
      label  of the server's network node.
      POST:   Sends data in chunks to server.  The server should then take care
      of routing.
    */
        
    virtual CTChannelStatus synchronize(unsigned int numParticipants);

    unsigned int clientID() {return channel_m->tag();}
        
    bool connect(int timeoutSecs);
    void disconnect();
        
        
    unsigned int serverNodeLabel() {return nodeLabel_m;}
    void setServerNodeLabel(unsigned int label) {nodeLabel_m = label;}
};

#endif
