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
 *  AdminClient.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#include "ctnetwork/CTClient.h"
#include "ctnetwork/CTServer.h"
#include "ctnetwork/CTNetwork.h"
#include "internal/Private.h"
#include "util/parsing.h"
#include "util/misc.h"

CTClient::CTClient(CTChannel *channel) 
{
    node_m = NULL;
    channel_m = channel;
}



void CTClient::getServerInfo()
{
    char            **list;
    CTMessage       *rmsg, *msg;
    int                     num;
        
    // get server's network info
    msg = CTServer::getServerInfoMessage();
        
    if ( kCTChannelOK == sendMessage(msg, 0, NULL) )
    {
        ulm_err(("Error: Getting handshake response msg...\n"));
        if ( kCTChannelOK == channel_m->getMessage(&rmsg) )
        {
            //parse data for info:
            // Label=<node label>;Network=<network type>
            num = _ulm_parse_string(&list, rmsg->data(), 1, ";");
            if ( num > 2 )
            {
                sscanf(list[0], "Label=%u", &nodeLabel_m);
                sscanf(list[1], "Network=%u", &nodeType_m);     
                sscanf(list[2], "NumNodes=%u", &numNodes_m);    
                                
                ulm_err(("Label=%u;Network=%u;NumNodes=%u\n", nodeLabel_m, nodeType_m, numNodes_m));
            }
                                
            free_double_carray(list, num);
                        
            rmsg->release();
        }

    }
        
    msg->release();
}



CTChannelStatus CTClient::sendMessage(CTMessage *msg, unsigned int ctrlSize, char *control)
{
    return CTController::sendMessage(channel_m, msg, ctrlSize, control);
}



CTChannelStatus CTClient::broadcast(CTMessage *msg, unsigned int ctrlSize, char *control)
{
    return CTController::broadcast(msg, ctrlSize, control);
}


CTChannelStatus CTClient::allgather(unsigned int numParticipants, unsigned int datalen, 
                                    char *sendBuf, char *recvBuf)
{
/*
  PRE:    This implementation ignores sendBuf for CTClient.  Use this to gather data from
  network nodes.
  POST:   Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
  and participating CTClients. recvBuf[i] is data from node i and length datalen.
*/
    // create sync msg and send to local node. Uses fan in/fan out algorithm
    CTMessage                       *msg, *gomsg;
    CTChannelStatus         status;
    const char                      *data;
        
    status = kCTChannelOK;
    msg = new CTMessage(CTMessage::kNetwork, NULL, 0);
    msg->setNetworkCommand(CTServer::kAllgather);
    if ( NULL == msg)
        return kCTChannelInvalidMsg;
                
    msg->setRoutingType(CTMessage::kLocal);
    msg->setSendingClientID(channel_m->tag());
    status = sendMessage(msg, 0, NULL);
                                
        
    if ( kCTChannelOK == status )
    {
        // wait for GO msg
        if ( kCTChannelOK == (status = channel_m->getMessage(&gomsg)) )
        {
            //parse aggregate data.  recvBuf must have been  preallocated.
            data = gomsg->data();
            memcpy(recvBuf, data, gomsg->dataLength());
                        
            gomsg->release();
        }
        else
            ct_err( ("Error in getting ALLGATHER msg.  status = %d\n", status) );
    }
    else
        ct_err( ("Error in sending ALLGATHER msg.  status = %d\n", status) );
                
    msg->release();
        
    return status;
}




CTChannelStatus CTClient::allgatherv(unsigned int numParticipants, char *recvBuf)
{
/*
  PRE:    This implementation ignores sendBuf for CTClient.  Use this to gather data from
  network nodes.
  POST:   Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
  and participating CTClients. recvBuf[i] is data from node i and length datalen.
*/
    // create sync msg and send to local node. Uses fan in/fan out algorithm
    CTMessage                       *msg, *gomsg;
    CTChannelStatus         status;
    const char                      *data;
        
    status = kCTChannelOK;
    msg = new CTMessage(CTMessage::kNetwork, NULL, 0);
    msg->setNetworkCommand(CTServer::kAllgatherv);
    if ( NULL == msg)
        return kCTChannelInvalidMsg;
                
    msg->setRoutingType(CTMessage::kLocal);
    msg->setSendingClientID(channel_m->tag());
    status = sendMessage(msg, 0, NULL);
                                
        
    if ( kCTChannelOK == status )
    {
        // wait for GO msg
        if ( kCTChannelOK == (status = channel_m->getMessage(&gomsg)) )
        {
            //parse aggregate data.  recvBuf must have been  preallocated.
            data = gomsg->data();
            memcpy(recvBuf, data, gomsg->dataLength());
                        
            gomsg->release();
        }
        else
            ct_err( ("Error in getting ALLGATHER msg.  status = %d\n", status) );
    }
    else
        ct_err( ("Error in sending ALLGATHER msg.  status = %d\n", status) );
                
    msg->release();
        
    return status;
}




CTChannelStatus CTClient::scatterv(int msgType, unsigned int *datalen, const char **data)
{
    unsigned int            *labels, ctrlsz, nnodes, i, sz, bufsz;
    char                            *ctrl, *buffer, *ptr;
    CTMessage                       *msg;
    CTChannelStatus         status = kCTChannelOK;
        
    if ( !node_m )
    {
        getServerInfo();
        // create a node instance; this will mirror the server's node.
        node_m = CTNode::createNode(nodeType_m, nodeLabel_m, numNodes_m);
    }

    if ( node_m )
    {
        nnodes = node_m->numberOfNodes();
        if ( (labels = (unsigned int *)malloc(sizeof(unsigned int)*nnodes)) )
        {
            for ( i = 0; i < nnodes; i++ )
                labels[i] = i;
                        
            ctrl = node_m->initialControlData(&ctrlsz);
                        
            // create largest buffer we may need
            bufsz = sizeof(unsigned int)*(2*nnodes + 1);
            for ( i = 0; i < nnodes; i++ )
                bufsz += datalen[i];
                                
            buffer = (char *)malloc(bufsz);
            if ( NULL == buffer )
            {
                // need error reporting here!
                return kCTChannelError;
            }

            // copy count of subtree
            ptr = buffer;
            memcpy(ptr, &nnodes, sz = sizeof(unsigned int));
            ptr += sz;
                        
            // copy  labels
            memcpy(ptr, labels, sz = nnodes*sizeof(unsigned int));
            ptr += sz;
                        
            // copy  data len array
            memcpy(ptr, datalen, sz = nnodes*sizeof(unsigned int));
            ptr += sz;

            // ptr should point to place in buffer to copy data
            for ( i = 0; i < nnodes; i++ )
            {
                memcpy(ptr, data[i], datalen[i]);
                ptr += datalen[i];
            }
            msg = new CTMessage(msgType, buffer, bufsz);
            msg->setRoutingType(CTMessage::kScatterv);
                        
            // send control info first
            status = sendMessage(msg, ctrlsz, ctrl);
                                
            msg->release();
                        
            free(buffer);
            free(ctrl);
        }
        free(labels);
    }
        
    return status;
}



CTChannelStatus CTClient::synchronize(unsigned int numParticipants)
{
    // create sync msg and send to local node. Uses fan in/fan out algorithm
    CTMessage                       *msg, *gomsg;
    CTChannelStatus         status;
        
    status = kCTChannelOK;
    msg = new CTMessage(CTMessage::kNetwork, NULL, 0);
    msg->setNetworkCommand(CTServer::kSynchronize);
    if ( NULL == msg)
        return kCTChannelInvalidMsg;
                
    msg->setRoutingType(CTMessage::kLocal);
    msg->setSendingClientID(channel_m->tag());
    status = sendMessage(msg, 0, NULL);
                                
        
    if ( kCTChannelOK == status )
    {
        // wait for GO msg
        if ( kCTChannelOK == (status = channel_m->getMessage(&gomsg)) )
        {
            gomsg->release();
        }
        else
            ct_err( ("Error in getting GO msg.  status = %d\n", status) );
    }
    else
        ct_err( ("Error in sending SYNC msg.  status = %d\n", status) );
                
    msg->release();
        
    return status;
}


bool CTClient::connect(int timeoutSecs)
{
    bool                    success;
    unsigned int    tag;
    long int                lenrcvd;
        
    success = channel_m->openChannel(timeoutSecs);
        
    if ( false == success )
        disconnect();
    else
    {
        // retrieve our assigned clientID
        if ( kCTChannelOK != channel_m->receive((char *)&tag, sizeof(tag), &lenrcvd) )
        {
            // do some error reporting
            success = false;
            disconnect();
        }
        else
        {
            ulm_err(("Error: Client setting tag to %d\n", tag));
            channel_m->setTag(tag);
        }
    }
                
    return success;
}


void CTClient::disconnect()
{
    channel_m->closeChannel();
}
