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
 *  CTServer.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#include <strings.h>

#include "CTServer.h"
#include "threads/Thread.h"
#include "util/misc.h"
#include "internal/log.h"
#include "internal/malloc.h"

#define BLOCK_SZ                16


#define ct_svr_err(x)   do {\
    _ulm_err ("[%s:%d] ", _ulm_host(), getpid()); \
    _ulm_err ("Server Node %d: ", node_m->label()); \
    _ulm_set_file_line(__FILE__, __LINE__) ; _ulm_err x ; \
    fflush(stderr); } while (0)

#ifdef CT_DEBUG
#define ct_svr_dbg(x)           ct_svr_err(x)
#else
#define ct_svr_dbg(x)
#endif

typedef struct
{
    void        *object;
    bool        isValid;
}  chnode_t;


static int _lookup_client(void *item1, void *clientID)
{
    CTChannel   *chnl1;

    chnl1 = (CTChannel *)item1;
    return ( chnl1->tag() == *(unsigned int *)clientID );
}


static void _dist_client(link_t *item, void *arg)
{
    CTChannel           *client;

    client = (CTChannel *)item->data;
    client->sendMessage((CTMessage *)arg);
}


static void _del_chnl(void *chnl)
{
    delete (CTChannel *)chnl;
}



#ifdef __cplusplus
extern "C"
{
#endif

    static char         _host[50];

    const char *_ulm_host()
    {
        return _host;
    }


    void CTNetworkInit()
    {
        static bool             flg = false;

        if ( true == flg ) return;

        gethostname(_host, 50);

        flg = true;
        CTServer::CTServerInit();
        CTChannel::CTChannelInit();
    }

#ifdef __cplusplus
}
#endif

/*
 *
 *              private methods
 *
 */


typedef bool (CTServer::*admin_fn_t)(CTChannel *, CTMessage *, unsigned int, char *);

#define ADMIN_VTBL_SZ           15
static  admin_fn_t              admin_vtbl[ADMIN_VTBL_SZ];

void CTServer::CTServerInit()
{
    // set up vtable for handling network msgs.
    bzero(admin_vtbl, sizeof(admin_fn_t)*ADMIN_VTBL_SZ);
    admin_vtbl[CTServer::kLinkNetwork] = (admin_fn_t)&CTServer::linkNetwork;
    admin_vtbl[CTServer::kGetServerInfo] = (admin_fn_t)&CTServer::getServerInfo;
    admin_vtbl[CTServer::kAllgather] = &CTServer::allgather;
    admin_vtbl[CTServer::kAllgatherv] = &CTServer::allgather;
    admin_vtbl[CTServer::kSynchronize] = &CTServer::synchronize;
}


/*
*               Channel mgmt
*/
bool CTServer::checkClientStatus(CTChannelStatus status, link_t *client)
{
        if ( kCTChannelOK ==  status )
                return true;

        if ( (kCTChannelError == status) ||
                        (kCTChannelClosed == status) ||
                        (kCTChannelConnLost == status) )
        {
                // remove client from list
                spinlock(&lock_m);
                clients_m = delete_link(clients_m, client, _del_chnl);
                spinunlock(&lock_m);
        }
        return false;
}

/*
 *              Network admin tasks
 */


void CTServer::connectToNeighbor(unsigned int neighborLabel, const char *connectInfo)
{
    CTChannel           *chnl;
    unsigned int        tag;
    long int            lenrcvd;

    if ( NULL == node_m ) return;

    chnl = CTChannel::createChannel(chserver_m->channel()->className(), connectInfo);
    if ( chnl )
    {
        chnl->openChannel();

        // retrieve our assigned clientID
        if ( kCTChannelOK != chnl->receive((char *)&tag, sizeof(tag), &lenrcvd) )
        {
            // do some error reporting
            ct_svr_err( ("Error in receiving tag ID from neighbor.\n") );
        }
        else
        {
            chnl->setTag(tag);

            node_m->addNeighbor(neighborLabel, chnl);           /* node is responsible for deleting chnl. */
            ct_svr_dbg( ("Added neighbor %d\n", neighborLabel) );
        }
    }
    else
    {
        ct_svr_err( ("Error: unable to create channel for neighbor %d.\n", neighborLabel) );
    }
}


void CTServer::getServerInfo(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl)
{
        CTMessage       *rmsg;
        char            buf[40];

        if ( node_m )
                sprintf(buf, "Label=%u;Network=%u;NumNodes=%u", node_m->label(), node_m->type(), node_m->numberOfNodes());
        else
                sprintf(buf, "Label=0;Network=0;NumNodes=0");
        if ( (rmsg = new CTMessage(CTMessage::kUser, buf, strlen(buf)+1)) )
        {
                chnl->sendMessage(rmsg);
                rmsg->release();
        }
        else
        {
                //return an error
        ulm_err(("Node %d: Unable to create msg. Sending error 1.\n", node_m->label()));
                chnl->sendError(1);
        }
}



bool CTServer::linkNetwork(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl)
{
    bool                success = true;
    char                ntype;
    unsigned int        nNodes, sz;
    unsigned int        label, myLabel, hsize, idx, lnk;
    const char          *ptr, *connptr;
    CTNode              *node;
    unsigned int        cnt, *links, i, nlabel, csz;
    char                *newctrl;
    CTChannel           *chnl;
    CTChannelStatus     status;

    /*
      ASSERT: msg data layout should look like (refer to linkNetworkMessage()):
      <networkType (char)>
      <numNodes (int)><node labels array><node connect info array>
    */
    ptr = msg->data();
    if ( NULL == ptr ) return false;

    memcpy(&ntype, ptr, sz = sizeof(ntype));
    ptr += sz;

    memcpy(&nNodes, ptr, sz = sizeof(nNodes));
    ptr += sz;

    ct_svr_dbg( ("Linking network of %d nodes.\n", nNodes) );

    /* create appropriate node for network topology. */
    node = NULL;
    switch ( ntype )
    {
    case CTNode::kHypercube:
        /* compute node's label:
           label = sending node label XOR (control + 1)
        */
        myLabel = 0;
        hsize = ulm_log2(nNodes);

        // since we are using bcast algorithm, we need to be careful about
        // the control structure because the higher order bits could be set if
        // those links don't exist.
        lnk = *(unsigned int *)ctrl + 1;
        if ( lnk < (unsigned int)(1 << hsize) )
        {
            myLabel = msg->sendingNode() ^ lnk;
        }
        node = (CTNode*) new HypercubeNode(myLabel, nNodes);
        node->setNeighborDeleteMethod(_del_chnl);
        setNode(node);          /* CTServer instance is responsible for deleting. */

        break;
    }

    if ( NULL == node )
        success = false;
    else
    {
        /*
          set up neighbors.
          Scan labels array and check which label identifies a neighbor.  Then,
          retrieve the neighbors connection info.
          ASSERT: Each connection info in packed array is a string.
          ptr points to beginning of labels array.
          connection[i] is connect info for node with label labels[i]
        */

        // compute beginning of connection info array
        connptr = ptr + nNodes * sizeof(unsigned int);
        idx = 0;
        while ( idx < nNodes )
        {
            // get label
            memcpy(&label, ptr, sz = sizeof(label));
            ptr += sz;

            if ( true == node->isANeighbor(label) )
            {
                //  now, create channel for corresponding connection info.
                // channel should be same as server's
                connectToNeighbor(label, connptr);
            }

            // jump to next connect info element
            connptr += (strlen(connptr) + 1);
            idx++;
        }

        // now send to all appropriate neighbors
        // Can't use bcast algorithm because it assumes a linked network.
        // get list of nodes to broadcast to
        links = node_m->broadcastList(ctrl, &cnt);
        if ( cnt )
        {
            myLabel = node_m->label();
            msg->setSendingNode(myLabel);
            for ( i = 0; i < cnt; i++ )
            {
                newctrl = node_m->controlForLinking(ctrl, links[i], &csz);
                // forward msg along link links[i]; get the channel for the neighbor on link links[i].
                nlabel = node_m->labelForLink(links[i]);
                ct_svr_dbg( ("broadcasting linkup msg to neighbor %d.\n", nlabel) );
                chnl = (CTChannel *)node_m->neighbor(nlabel);
                if ( chnl )
                {
                    status = CTController::sendMessage(chnl, msg, csz, newctrl);
                    if (status != kCTChannelOK) {
                        ct_svr_err(("Error in sending msg. status = %d.\n", status));
                    }
                }
                else
                {
                    ulm_err(("Node %d: Error: neighbor %d does not exist.\n", myLabel, nlabel));
                }

                free(newctrl);
            }
        }
    }

    return success;
}


bool CTServer::allgather(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl)
{
    link_t                      *item;
    bool                        success;
    const char          *data;
    unsigned int        datalen, offset;
    char                        cmd;
    int                         totalsz, bufsz;

    success = true;
    cmd = msg->networkCommand();
    if ( msg->sendingClientID() )
    {
        if ( schnl->tag() == msg->sendingClientID() )
        {
            // msg came from direct connection by CTClient; save to queue
            item = newlink(schnl);
            spinlock(&coll_lock_m);
            collClients_m[COLL_IDX(cmd)] = addfront(collClients_m[COLL_IDX(cmd)], item);
            spinunlock(&coll_lock_m);
        }

        // If this is node 0, then increase client cnt; otherwise, forward msg onto node 0.
        if ( 0 == node_m->label() )
        {
            spinlock(&coll_lock_m);
            clientCnt_m[COLL_IDX(cmd)]++;
            spinunlock(&coll_lock_m);
        }
        else
        {
            // Now forward onto node 0.
            msg->setDestination(0);
            msg->setRoutingType(CTMessage::kPointToPoint);
            sendMessage(msg);
        }
    }
    else
    {
        data = msg->data();
        datalen = msg->dataLength();

                if ( 0 == msg->sourceNode() )
                {
                        // We have received the aggregate data.
                        // Copy data to buffer.
                        // Since node 0 also receives this msg, do not bother with the copy.
                        spinlock(&coll_lock_m);
            if ( NULL == buffer_m[COLL_IDX(cmd)] )
            {
                buffer_m[COLL_IDX(cmd)] = (char *)malloc(datalen);
                if ( !buffer_m[COLL_IDX(cmd)] )
                {
                    ulm_err(("Error: Can't alloc memory for buffer (cmd = %d).\n", cmd));
                    spinunlock(&coll_lock_m);
                    return false;
                }
            }
            memcpy(buffer_m[COLL_IDX(cmd)], data, datalen);

            // Now set flag to continue.
            collOK_m[COLL_IDX(cmd)] = true;

                        // distribute msg to CTClients
                        apply(collClients_m[COLL_IDX(cmd)], _dist_client, msg);
                        freeall_with(collClients_m[COLL_IDX(cmd)], NULL);
                        collClients_m[COLL_IDX(cmd)] = NULL;

                        spinunlock(&coll_lock_m);
                }
                else
                {
                        // ASSERT: This node is 0.
                        // aggregate node data in buffer.
                        spinlock(&coll_lock_m);
            if ( CTServer::kAllgatherv == cmd )
            {
                // allgatherv data layout: <total size (int)><node's offset (int)><node's data>
                memcpy(&totalsz, data, sizeof(totalsz));
                data += sizeof(totalsz);
                datalen -= sizeof(totalsz);

                memcpy(&offset, data, sizeof(offset));
                data += sizeof(offset);
                datalen -= sizeof(offset);

                bufsz = totalsz;
            }
            else
            {
                bufsz = datalen * node_m->numberOfNodes();
                offset = msg->sourceNode() * datalen;
            }

            if ( NULL == buffer_m[COLL_IDX(cmd)] )
            {
                buffer_m[COLL_IDX(cmd)] = (char *)malloc(bufsz);
                if ( !buffer_m[COLL_IDX(cmd)] )
                {
                    ulm_err(("Error: Can't alloc memory for buffer (cmd = %d).\n", cmd));
                    spinunlock(&coll_lock_m);
                    return false;
                }
            }
            memcpy(buffer_m[COLL_IDX(cmd)] + offset, data, datalen);

            childCnt_m[COLL_IDX(cmd)]++;
            spinunlock(&coll_lock_m);
        }
    }

    return success;
}



bool CTServer::synchronize(CTChannel *schnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl)
{
    // check if msg came from CTClient connection.  If so, then queue the client for
    // sending sync response.  Then relay CTClient msg to node 0.
    // sync msg data: <sync cmd><# of participants>
    link_t                      *item;
    bool                        success;

    // check if this is the OK msg. If so, then relay to all CTClients.
    // OK msg iff source node is node 0.

    // Do not proceed with algorithm until we have entered synchronize() call.
    // So spin on _syncing flag.

    success = true;
    if ( msg->sendingClientID() )
    {
        if ( schnl->tag() == msg->sendingClientID() )
        {
            ct_svr_dbg( ("Adding client %d to sync queue.\n", msg->sendingClientID()) );
            // msg came from direct connection by CTClient; save to queue
            item = newlink(schnl);
            spinlock(&coll_lock_m);
            collClients_m[COLL_IDX(kSynchronize)] = addfront(collClients_m[COLL_IDX(kSynchronize)], item);
            spinunlock(&coll_lock_m);
        }

        // If this is node 0, then increase client cnt; otherwise, forward msg onto node 0.
        if ( 0 == node_m->label() )
        {
            spinlock(&coll_lock_m);
            clientCnt_m[COLL_IDX(kSynchronize)]++;
            spinunlock(&coll_lock_m);
        }
        else
        {
            // Now forward onto node 0.
            ct_svr_dbg( ("Forwarding sync client %d to node 0.\n", msg->sendingClientID()) );
            msg->setDestination(0);
            msg->setRoutingType(CTMessage::kPointToPoint);
            sendMessage(msg);
        }
    }
    else
    {
        if ( 0 == msg->sourceNode() )
        {
            // this is the OK msg from parent.  Now check whether we need to send
            // to CTClients.  The bcast algorithm will relay msg to children.
            spinlock(&coll_lock_m);
            collOK_m[COLL_IDX(kSynchronize)] = true;

            //Distribute to CTClients
            apply(collClients_m[COLL_IDX(kSynchronize)], _dist_client, msg);
            freeall_with(collClients_m[COLL_IDX(kSynchronize)], NULL);
            collClients_m[COLL_IDX(kSynchronize)] = NULL;

            // reset for next sync
            childCnt_m[COLL_IDX(kSynchronize)] = 0;
            clientCnt_m[COLL_IDX(kSynchronize)] = 0;
            spinunlock(&coll_lock_m);
        }
        else
        {
            spinlock(&coll_lock_m);
            childCnt_m[COLL_IDX(kSynchronize)]++;
            spinunlock(&coll_lock_m);
        }
    }

    return success;
}






/*
 *              Message routing
 */

void CTServer::broadcastPackedMessage(char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl)
{
        unsigned int    cnt, *links, i, nlabel, myLabel, csz;
        char                    *newctrl;
        CTChannel               *chnl;
        char                    flg = 1;
        CTChannelStatus status;
        long int                rlen;
    int                         cmd = CTMessage::getNetworkCommand(pmsg);

    // get list of nodes to broadcast to
        links = node_m->broadcastList(ctrl, &cnt);
        if ( cnt )
        {
                myLabel = node_m->label();
                CTMessage::setSendingNode(pmsg, myLabel);
                for ( i = 0; i < cnt; i++ )
                {
                        newctrl = node_m->controlDataForSending(ctrl, links[i], &csz);
                        // forward msg along link links[i]; get the channel for the neighbor on link links[i].
                        nlabel = node_m->labelForLink(links[i]);
                        chnl = (CTChannel *)node_m->neighbor(nlabel);
                        if ( chnl )
                        {
                                // send control info first.
                                //send ctrl flag
                                status = chnl->sendData(&flg, sizeof(flg), &rlen);

                                // send ctrl len
                if ( kCTChannelOK == status )
                    status = chnl->sendData((char *)&ctrlSize, sizeof(ctrlSize), &rlen);

                                // send ctrl info
                if ( kCTChannelOK == status )
                    status = chnl->sendData(newctrl, ctrlSize, &rlen);

                if ( kCTChannelOK == status )
                    status = chnl->sendPackedMessage(pmsg, msglen);

                if ( kCTChannelOK != status )
                    ulm_err(("Node %d: Error in bcasting msg (cmd = %d) to neighbor %d."
                             "status = %d.\n", myLabel, cmd, nlabel));
                        }
                        else
                        {
                                ulm_err(("Node %d: Error: neighbor %d does not exist.\n", myLabel, nlabel));
                        }

                        free(newctrl);
                }
        }
}


void CTServer::handleMessage(CTChannel *chnl, char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl)
{
    CTMessage           *msg;
    CTChannel           *client;
    unsigned int        cid;
    link_t                      *item;

    msg = CTMessage::unpack(pmsg);

    if ( NULL == msg )
    {
        ct_svr_err( ("Unable to unpack msg.\n") );
        return;
    }

    /* check if user or network msg. */
    if ( CTMessage::kUser == msg->type() )
    {
        // check if msg is for a connected CTClient
        // ASSERT: all connected CTClients should have clientID > 0
        if ( msg->destinationClientID() )
        {
            // search list of client connections for matching clientID
            spinlock(&lock_m);
            cid = msg->destinationClientID();
            if ( (item = lookup(clients_m, _lookup_client, &cid)) )
            {
                client = (CTChannel *)item->data;
                client->sendPackedMessage(pmsg, msglen);
            }
            spinunlock(&lock_m);


        }
        else if ( delegate_m )
            delegate_m->messageDidArrive(this, msg);
    }
    else
        handleNetMsg(chnl, msg, ctrlSize, ctrl);

    msg->release();
}


void CTServer::handleNetMsg(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *ctrl)
{
    char        ncmd;
    admin_fn_t  fn;
    const char  *data;

    /*
      ASSERT: msg data layout should follow:
      <network command specific info>
      msg->networkCommand() is one of enum values for  CTServer, e.g. kLinkNetwork
    */
    data = msg->data();
    if (!data) {
        /* error ? */
    }
    ncmd = msg->networkCommand();

    // get handler method from vtable.
    if ( (ncmd < ADMIN_VTBL_SZ) && (fn = admin_vtbl[ncmd]) )
    {
        (this->*fn)(chnl, msg, ctrlSize, ctrl);
    }
    else
    {
        ct_svr_err( ("Error: net command %d is not available.\n", ncmd) );
    }
}


void CTServer::routeMessage(CTChannel *chnl, char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl)
{
    int             rtype;
    unsigned int    dst, nextNeighbor;
    bool            msgDropped = false;
    CTChannel       *nchnl;
    CTMessage       *msg;
    CTChannelStatus status;
#ifdef FIX_ME_ROB
    int             cmd;
#endif

    /* check if we should handle the msg. */
    rtype = CTMessage::getRoutingType(pmsg);

#ifdef FIX_ME_ROB
    cmd = CTMessage::getNetworkCommand(pmsg);
#endif

    /* check type of route to see if we need to forward
       msg. */
    switch ( rtype )
    {
    case CTMessage::kLocal:
        handleMessage(chnl, pmsg, msglen, ctrlSize, ctrl);
        break;
    case CTMessage::kPointToPoint:
        if ( ( true == CTMessage::getDestination(pmsg, &dst)) && node_m )
        {
            if ( node_m->label() == dst )
            {
                handleMessage(chnl, pmsg, msglen, ctrlSize, ctrl);
            }
            else
            {
                /* forward msg to next svr in path. */

                nextNeighbor = node_m->nextNeighborLabelToNode(dst);
                if ( (nchnl = (CTChannel *)node_m->neighbor(nextNeighbor)) )
                {
                    CTController::sendControlField(nchnl, 0, NULL);
                    CTMessage::setSendingNode(pmsg, node_m->label());
                    status = nchnl->sendPackedMessage(pmsg, msglen);
                    if ( kCTChannelOK != status )
                        ulm_err(("Node %d: Unable to forward msg from %d to next neighbor %d. status = %d.\n",
                                 node_m->label(), CTMessage::getSourceNode(pmsg), nextNeighbor, status));

                }
                else
                {
                    ulm_err(("Node %d: Unable to forward msg. No channel for neighbor %d.\n",
                             node_m->label(), nextNeighbor));
                }
            }
        }
        else
            msgDropped = true;
        break;
    case CTMessage::kScatterv:
        scattervPackedMessage(pmsg, msglen, ctrlSize, ctrl);
        break;
    case CTMessage::kLinkup:
        msg = CTMessage::unpack(pmsg);
        if ( msg )
        {
            if ( true == linkNetwork(chnl, msg, ctrlSize, ctrl) )
                hasLinkedNet_m = true;
            msg->release();
        }
        else
        {
            ulm_err(("Node %d: Invalid message during linkup.  Unable to proceed.", node_m->label()));
        }
        break;
    case CTMessage::kBroadcast:
        broadcastPackedMessage(pmsg, msglen, ctrlSize, ctrl);
        handleMessage(chnl, pmsg, msglen, ctrlSize, ctrl);
        break;
    default:
        break;
    }

    if ( true == msgDropped )
    {
        // report an error
        CTMessage::getDestination(pmsg, &dst);
        ulm_err(("Node %d: Error: Dropping msg destined for node %d\n", node_m->label(), dst));
    }
}


void CTServer::scattervPackedMessage(char *pmsg, long int msglen, unsigned int ctrlSize, char *ctrl)
{
    const char          *data, **ndata;
    unsigned int        *lptr, *dlptr;
    unsigned int        type, bufsz, source;
    unsigned int        cnt, sz, i;

    /* get pointer to packed data. Jump to node's data.
       ASSERT: scatterv data is arranged as:
       <num items(unsigned int)><array of node labels for subcube><datalen array (array of unsigned int)><node data array>
       - labels[i] is label of node to receive data[i]
    */

    if ( NULL == node_m )
    {
        //need some error here
        return;
    }

    //get pointer to data segment in packed msg and get count of nodes.
    data = CTMessage::getData(pmsg);

    memcpy(&cnt, data, sz = sizeof(cnt));
    data += sz;

    if ( 0 == cnt )
        return;         // nothing to do

    /*
      Preprocessing: find pointers in data so that data[i] is data for node label[i].
    */
    if ( NULL == (ndata = (const char **)malloc(sizeof(char *)*cnt)) )
    {
        return;
    }
    lptr = (unsigned int *)data;        // label array
    dlptr = lptr + cnt;                                 // datalen array (pointer math should work correctly and skip over cnt integers)
    ndata[0] = (char *)(dlptr + cnt);   // start of data array
    bufsz = dlptr[0];
    for ( i = 1; i < cnt; i++ )
    {
        ndata[i] = ndata[i-1] + dlptr[i-1];
        bufsz += dlptr[i];
    }


    /*  ASSERT: ndata[i] is pointer to data for node lptr[i] and has length dlptr[i]. */

    type = CTMessage::getMessageType(pmsg);
    source = CTMessage::getSourceNode(pmsg);
    if ( false == scatterv(source, type, cnt, lptr, dlptr, ndata, ctrl, ctrlSize) )
    {
        ct_svr_err( ("Unable to scatter msg!\n") );
    }
}


bool CTServer::scatterv(unsigned int sourceLabel, int msgType, unsigned int labelCnt, unsigned int *labels,
                        unsigned int *datalen, const char **data, char *ctrl, unsigned int ctrlsz)
{
    char                *offset, *buffer, *ptr, *newctrl;
    unsigned int        *links, *counts, arrayCnt, **scatterList;
    CTChannel           *chnl;
    bool                err;
    CTChannelStatus     status;
    unsigned int        bufsz, idx, j;
    unsigned int        label, sz, i;
    CTMessage           *msg;

    err = true;
    status = kCTChannelOK;
    if ( NULL == node_m )
    {
        //need some error here
        return false;
    }

    /*  ASSERT: data[i] is pointer to data for node labels[i] and has length datalen[i]. */

    // find current node's data and rout msg appropriately.
    for ( i = 0; i < labelCnt; i++ )
    {
        if ( node_m->label() == labels[i] )
        {
            msg = new CTMessage(msgType, data[i], datalen[i]);
            msg->setRoutingType(CTMessage::kScatterv);
            msg->setSourceNode(sourceLabel);
            /* route msg appropriately. */
            /* check if user or network msg. */
            if ( CTMessage::kUser == msgType )
            {
                if ( delegate_m )
                    delegate_m->messageDidArrive(this, msg);
            }
            else
                handleNetMsg(NULL, msg, ctrlsz, ctrl);

            msg->release();
            break;
        }
    }

    /*
      Now handle rest of messages.
      The scatter logic is similar to broadcasting in that it uses the control data
      to determine the set of links across which it should partition the node data.
    */
    scatterList = node_m->scatterList(ctrl, labels, labelCnt, &links, &counts, &arrayCnt);

    /*
      ASSERT: - scatterList[i] is the subtree generated by following along link links[i].
      - scatterList[i][j] < scatterList[i][j+1]
      - counts[i] >= counts[i+1]
      We want to forward data for all nodes in scatterList[i] for each i.
    */

    // create largest buffer we may need
    bufsz = sizeof(unsigned int)*(2*labelCnt + 1);
    for ( i = 0; i < labelCnt; i++ )
        bufsz += datalen[i];

    buffer = (char *)malloc(bufsz);
    if ( NULL == buffer )
    {
        // need error reporting here!
        return false;
    }

    for ( i = 0; i < arrayCnt; i++ )
    {
        // create the msg to send along link links[i]

        // copy count of subtree
        ptr = buffer;
        memcpy(ptr, &counts[i], sz = sizeof(unsigned int));
        ptr += sz;

        // copy subtree labels
        memcpy(ptr, scatterList[i], sz = counts[i]*sizeof(unsigned int));
        ptr += sz;

        offset = ptr + counts[i]*sizeof(unsigned int);  // this will point to place in buffer to copy the data

        idx = 0;
        err = false;
        for ( j = 0; j < counts[i]; j++ )
        {
            // here, we use the fact that scatterList[i][j] < scatterList[i][j+1]
            while ( idx < labelCnt )
            {
                // find index in data array for node with label scatterList[i][j]
                if ( labels[idx] == scatterList[i][j] )
                {
                    break;
                }
                idx++;
            }
            if ( idx < labelCnt )
            {
                // copy data len; ptr should point to place in buffer to copy the data length array
                memcpy(ptr, datalen + idx, sizeof(unsigned int));
                ptr += sizeof(unsigned int);

                // copy data
                memcpy(offset, data[idx], datalen[idx]);
                offset += datalen[idx];
            }
            else
            {
                // need warning here!
                ct_svr_err( ("Warning: label %d not found in scattering list!\n", scatterList[i][j]) );
                err = true;
            }
        }

        if ( false == err )
        {
            // size of msg data should be offset - buffer since offset points to data area.
            msg = new CTMessage(msgType, buffer, offset - buffer);
            msg->setSourceNode(sourceLabel);
            msg->setSendingNode(node_m->label());
            msg->setRoutingType(CTMessage::kScatterv);

            // get the channel for links[i] and send msg.
            label = node_m->labelForLink(links[i]);
            chnl = (CTChannel *)node_m->neighbor(label);
            if ( chnl )
            {
                // send new control info first.
                if ( (newctrl = node_m->controlDataForSending(ctrl, links[i], &ctrlsz)) )
                {
                    status = CTController::sendMessage(chnl, msg, ctrlsz, newctrl);
                    if (status != kCTChannelOK) {
                        ct_svr_err(("Error in sending msg. status = %d.\n", status));
                    }
                }
                else
                {
                    ct_svr_err( ("Error: unable to relay scatterv msg to node %d."
                                 " Unable to create new control structure!\n", label) );
                }

                free(newctrl);
            }
            else
            {
                ct_svr_err( ("Error: unable to relay scatterv msg to node %d."
                             " Channel does not exist!\n", label) );
            }

            msg->release();
        }
    }           // end of for loop

    free(buffer);

    return true;
}



/*
 *
 *              public/protected methods
 *
 */

#define DEFAULT_NODE            CTNode::kHypercube

CTServer::CTServer(CTChannelServer *channel)
    : CTController(),
      hasLinkedNet_m(false),
      isStarted_m(false),
      performingChecks_m(false)
{
    netxtID_m = 1;
    chserver_m = channel;
    bzero(collClients_m, CT_SVR_MAX_COLL*sizeof(link_t *));
    bzero(childCnt_m, CT_SVR_MAX_COLL*sizeof(unsigned int));
    bzero(clientCnt_m, CT_SVR_MAX_COLL*sizeof(unsigned int));
    bzero(buffer_m, CT_SVR_MAX_COLL*sizeof(char *));

    channel_m = chserver_m->channel();
    CTController::delegate_m = NULL;
    node_m = CTNode::createNode(DEFAULT_NODE, 0, 1);
    clients_m = NULL;

    coll_lock_m.data.lockData_m = LOCK_UNLOCKED;
    lock_m.data.lockData_m = LOCK_UNLOCKED;
    scanlock_m.data.lockData_m = LOCK_UNLOCKED;
}



CTServer::~CTServer()
{
    freeall_with(clients_m, _del_chnl);

    delete chserver_m;
    delete node_m;
}



CTMessage *CTServer::linkNetworkMessage(int networkType, int numNodes, unsigned int *nodeLabels,
                                        const char **nodeConnectInfo)
{
    CTMessage           *msg;
    char                        *data, ntype;
    long int            datalen, sz, len, i;

    /*
      packed layout for specifying link info:
      <networkType (char)>
      <numNodes (int)><node labels array><node connect info array>
    */

    msg = NULL;
    len = sizeof(char) + sizeof(numNodes) + numNodes*sizeof(unsigned int);
    for ( i = 0; i < numNodes; i++ )
        len += (strlen(nodeConnectInfo[i]) + 1);

    /*
      ASSUMPTION: we're initiating linkup by sending this linkup msg to node 0.
    */

    if ( (data = (char *)malloc(sizeof(char)*len)) )
    {
        datalen = 0;
        ntype = networkType;
        memcpy(data+datalen, &ntype, sz = sizeof(ntype));
        datalen += sz;
        memcpy(data+datalen, &numNodes, sz = sizeof(numNodes));
        datalen += sz;
        memcpy(data+datalen, nodeLabels, sz = numNodes*sizeof(unsigned int));
        datalen += sz;
        for ( i = 0; i < numNodes; i++ )
        {
            memcpy(data+datalen, nodeConnectInfo[i], sz = (strlen(nodeConnectInfo[i]) + 1));
            datalen += sz;
        }

        msg = new CTMessage(CTMessage::kNetwork, data, datalen);
        msg->setRoutingType(CTMessage::kLinkup);

        free(data);
    }

    return msg;
}



CTMessage *CTServer::getServerInfoMessage()
{
    CTMessage           *msg;

    /*
     */
    if ( (msg = new CTMessage(CTMessage::kNetwork, NULL, 0)) )
    {
        msg->setNetworkCommand(CTServer::kGetServerInfo);
        msg->setRoutingType(CTMessage::kLocal);
    }

    return msg;
}




bool CTServer::start()
{
    bool        success = true;

    if ( true == isStarted_m )
        return true;

    isStarted_m = true;
    /* start up the channel for accepting connections. */
    chserver_m->setupToAcceptConnections();

    /* spawn thread to accept connections. */
    Thread::createThread(this, (runImpl_t)&CTServer::acceptConnections, NULL);

    /* spawn thread to perform admin tasks. */
    if ( clients_m || (node_m && node_m->hasNeighbors()) )
        Thread::createThread(this, (runImpl_t)&CTServer::performChecks, NULL);

    return success;
}


void CTServer::stop()
{
    if ( false == isStarted_m  )
        return;

    isStarted_m = false;
    /* wake up threads to tell them to stop. */
    chserver_m->channel()->closeChannel();


}

CTChannelStatus CTServer::broadcast(CTMessage *msg, unsigned int ctrlSize, char *control)
{
    char                        *pmsg;
    long int            msglen;
    CTChannelStatus     status;

    if ( !node_m ) return kCTChannelError;
    status = kCTChannelOK;
    msg->setSourceNode(node_m->label());
    msg->setRoutingType(CTMessage::kBroadcast);
    if ( msg->pack(&pmsg, &msglen) )
    {
        broadcastPackedMessage(pmsg, msglen, ctrlSize, control);
        free(pmsg);
    }
    else
        status = kCTChannelInvalidMsg;

    return status;
}


CTChannelStatus CTServer::broadcast(CTMessage *msg)
{
    unsigned int        ctrlSize;
    char                        *control;

    if ( !node_m ) return kCTChannelError;
    control = node_m->initialControlData(&ctrlSize);
    return broadcast(msg, ctrlSize, control);
}


CTChannelStatus CTServer::sendMessage(CTMessage *msg)
{
    unsigned int        ctrlSize;
    long int            msglen;
    char                        *control, *pmsg;
    CTChannelStatus     status;

    if ( !node_m ) return kCTChannelError;

    status = kCTChannelOK;
    control = node_m->initialControlData(&ctrlSize);
    msg->setSourceNode(node_m->label());
    msg->setRoutingType(CTMessage::kPointToPoint);
    if ( msg->pack(&pmsg, &msglen) )
    {
        routeMessage(channel_m, pmsg, msglen, ctrlSize, control);
        free(pmsg);
    }
    else
        status = kCTChannelInvalidMsg;

    return status;
}


CTChannelStatus CTServer::allgather(unsigned int numParticipants, unsigned int datalen,
                                    char *sendBuf, char *recvBuf)
{
/*
  POST: Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
  and participating CTClients. recvBuf[i] is data from node i and length datalen.
*/
    // if this is not node 0, then send pt-to-pt message to node 0.  Then wait for bcast response
    // from node 0.
    unsigned int        sz, cnt;
    CTMessage           *msg;
    bool                isOK;
    CTChannelStatus     status = kCTChannelOK;

    if ( 0 != node_m->label() )
    {
        // format msg data as:
        // <datalen><data>
        sz = datalen;
        msg = new CTMessage(CTMessage::kNetwork, sendBuf, datalen, false);
        msg->setDestination(0);
        msg->setNetworkCommand(CTServer::kAllgather);

        ct_svr_dbg( ("Sending allgather msg to node 0.\n") );
        status = sendMessage(msg);
        if ( kCTChannelOK != status )
            ct_svr_err( ("Error in sending sync msg to node 0. status = %d.\n", status) );

        msg->release();
    }
    else
    {
        // wait until we have received from all clients.
        // Then bcast result.
        cnt = 0;
        while ( cnt != (numParticipants - 1) )
        {
            spinlock(&coll_lock_m);
            cnt = childCnt_m[COLL_IDX(kAllgather)] + clientCnt_m[COLL_IDX(kAllgather)];
            spinunlock(&coll_lock_m);
            usleep(10);
        }

        // Include node 0's data
        sz = datalen * node_m->numberOfNodes();
        spinlock(&coll_lock_m);
        if ( NULL == buffer_m[COLL_IDX(kAllgather)] )
            buffer_m[COLL_IDX(kAllgather)] = (char *)malloc(sz);
        memcpy(buffer_m[COLL_IDX(kAllgather)], sendBuf, datalen);

        childCnt_m[COLL_IDX(kAllgather)] = clientCnt_m[COLL_IDX(kAllgather)] = 0;

        // bcast aggregate data buffer
        msg = new CTMessage(CTMessage::kNetwork, buffer_m[COLL_IDX(kAllgather)], sz);
        msg->setNetworkCommand(CTServer::kAllgather);


        // send to node 0's CTClients
        apply(collClients_m[COLL_IDX(kAllgather)], _dist_client, msg);
        freeall_with(collClients_m[COLL_IDX(kAllgather)], NULL);
        collClients_m[COLL_IDX(kAllgather)] = NULL;

        collOK_m[COLL_IDX(kAllgather)] = true;
        spinunlock(&coll_lock_m);

        broadcast(msg);

        msg->release();
    }

    isOK = false;
    while ( !isOK )
    {
        spinlock(&coll_lock_m);
        isOK = collOK_m[COLL_IDX(kAllgather)];
        spinunlock(&coll_lock_m);
        usleep(10);
    }

    //parse aggregate data.  recvBuf must have been  preallocated.
    spinlock(&coll_lock_m);
    collOK_m[COLL_IDX(kAllgather)] = false;
    memcpy(recvBuf, buffer_m[COLL_IDX(kAllgather)], node_m->numberOfNodes() * datalen);
    free(buffer_m[COLL_IDX(kAllgather)]);
    buffer_m[COLL_IDX(kAllgather)] = NULL;
    spinunlock(&coll_lock_m);

    return status;
}




CTChannelStatus CTServer::allgatherv(unsigned int numParticipants, unsigned int sendlen,
                                                                                char *sendBuf, int *recvLens, char *recvBuf)
{
    /*
      POST: Gathers individual node data to node 0 then broadcasts
      aggregate data to all nodes and participating CTClients.
    */
    char            *buf;
    unsigned int    sz, cnt, cmd;
    int             totalsz;
    unsigned int    i;
    CTMessage       *msg;
    bool            isOK;
    int             offset;
    CTChannelStatus status;

    status = kCTChannelOK;
    cmd = CTServer::kAllgatherv;
    // compute the offset for this node
    totalsz = 0;
    for ( i = 0; i < node_m->label(); i++ )
    {
        totalsz += recvLens[i];
    }
    offset = totalsz;

    // compute rest of total size
    for ( i = node_m->label(); i < node_m->numberOfNodes(); i++ )
    {
        totalsz += recvLens[i];
    }

    if ( 0 != node_m->label() )
    {
        // format msg data as:
        // <total data len><node's offset (int)><data>
        sz = sizeof(totalsz) + sizeof(int) + sendlen;
        buf = (char *)malloc(sz);
        if ( !buf )
        {
            ulm_err(("Node %d: Unable to alloc mem.\n", node_m->label()));
            return kCTChannelMalloc;
        }

        memcpy(buf, &totalsz, sizeof(totalsz));
        memcpy(buf + sizeof(totalsz), &offset, sizeof(int));
        memcpy(buf + sizeof(totalsz)+ sizeof(int), sendBuf, sendlen);
        msg = new CTMessage(CTMessage::kNetwork, buf, sz, false);
        msg->setDestination(0);
        msg->setNetworkCommand(cmd);

        status = sendMessage(msg);
        if ( kCTChannelOK != status )
        {
            ulm_err( ("Node %d: Error in sending allgatherv msg to node 0. status = %d.\n",
                      node_m->label(), status) );
            return status;
        }

        msg->release();
        free(buf);
    }
    else
    {
        // wait until we have received from all clients.
        // Then bcast result.
        cnt = 0;
        while ( cnt != (numParticipants - 1) )
        {
            spinlock(&coll_lock_m);
            cnt = childCnt_m[COLL_IDX(cmd)] + clientCnt_m[COLL_IDX(cmd)];
            spinunlock(&coll_lock_m);
            usleep(10);
        }

        // Include node 0's data
        spinlock(&coll_lock_m);
        if ( NULL == buffer_m[COLL_IDX(cmd)] )
            buffer_m[COLL_IDX(cmd)] = (char *)malloc(totalsz);
        memcpy(buffer_m[COLL_IDX(cmd)], sendBuf, sendlen);

        childCnt_m[COLL_IDX(cmd)] = clientCnt_m[COLL_IDX(cmd)] = 0;

        // bcast aggregate data buffer
        msg = new CTMessage(CTMessage::kNetwork, buffer_m[COLL_IDX(cmd)], totalsz, false);
        msg->setNetworkCommand(cmd);


        // send to node 0's CTClients
        apply(collClients_m[COLL_IDX(cmd)], _dist_client, msg);
        freeall_with(collClients_m[COLL_IDX(cmd)], NULL);
        collClients_m[COLL_IDX(cmd)] = NULL;

        collOK_m[COLL_IDX(cmd)] = true;
        spinunlock(&coll_lock_m);

        status = broadcast(msg);

        msg->release();
    }

    isOK = false;
    while ( !isOK )
    {
        spinlock(&coll_lock_m);
        isOK = collOK_m[COLL_IDX(cmd)];
        spinunlock(&coll_lock_m);
        usleep(10);
    }

    //parse aggregate data.  recvBuf must have been  preallocated.
    spinlock(&coll_lock_m);
    collOK_m[COLL_IDX(cmd)] = false;

    /* allgatherv msg data layout:
       < data array len array (int *)><data buffer>
    */

    memcpy(recvBuf, buffer_m[COLL_IDX(cmd)], totalsz);
    free(buffer_m[COLL_IDX(cmd)]);
    buffer_m[COLL_IDX(cmd)] = NULL;
    spinunlock(&coll_lock_m);


    return status;
}


CTChannelStatus CTServer::scatterv(int msgType, unsigned int *datalen, const char **data)
{
    unsigned int                *labels, ctrlsz, nnodes, i;
    char                        *ctrl;

    nnodes = node_m->numberOfNodes();
    if ( (labels = (unsigned int *)malloc(sizeof(unsigned int)*nnodes)) )
    {
        for ( i = 0; i < nnodes; i++ )
            labels[i] = i;

        ctrl = node_m->initialControlData(&ctrlsz);
        scatterv(node_m->label(), msgType, nnodes, labels, datalen, data, ctrl, ctrlsz);
        free(ctrl);
    }
    free(labels);

    return kCTChannelOK;
}


CTChannelStatus CTServer::synchronize(unsigned int numParticipants)
{
    /*
      synchronize uses fan in/fan out algorithm.  Sends kSynchronize cmd to parent and
      then waits for response from node 0.
    */
    bool                isOK;
    unsigned int        cnt;
    CTMessage           *msg;
    CTChannelStatus     status;
    unsigned int        totalCnt = 0;

    spinlock(&coll_lock_m);
    isOK = collOK_m[COLL_IDX(kSynchronize)] = false;
    spinunlock(&coll_lock_m);

    if ( 0 == node_m->label() )
    {
        totalCnt = numParticipants - 1;
    }

    /*
      If we are a leaf, then initiate sync by sending sync message to parent.
      Otherwise, wait for GO msg.
    */
    if ( 0 != node_m->label() )
    {
        // just send to node 0
        msg = new CTMessage(CTMessage::kNetwork, NULL, 0);
        msg->setDestination(0);
        msg->setNetworkCommand(CTServer::kSynchronize);
        status = sendMessage(msg);
        if ( kCTChannelOK != status )
        {
            ulm_err( ("Node %d: Error in sending sync msg to node 0. status = %d.\n",
                      node_m->label(), status) );
            return status;
        }

        msg->release();
    }
    else
    {
        // main loop for interior node in spanning tree.  Wait until we recvd from
        // all child nodes.
        cnt = 0;
        while ( cnt != totalCnt )
        {
            spinlock(&coll_lock_m);
            cnt = childCnt_m[COLL_IDX(kSynchronize)] + clientCnt_m[COLL_IDX(kSynchronize)];
            spinunlock(&coll_lock_m);
            usleep(10);
        }

        spinlock(&coll_lock_m);

        collOK_m[COLL_IDX(kSynchronize)] = true;

        // Now broadcast sync msg back to network.  This will serve as a GO msg.
        childCnt_m[COLL_IDX(kSynchronize)] = clientCnt_m[COLL_IDX(kSynchronize)] = 0;
        msg = new CTMessage(CTMessage::kNetwork, NULL, 0);
        msg->setSourceNode(node_m->label());
        msg->setNetworkCommand(CTServer::kSynchronize);
        status = broadcast(msg);
        if ( kCTChannelOK != status )
            ulm_err( ("Node 0: Error in broadcasting sync GO msg. status = %d.\n", status) );

        // send to node 0's clients
        apply(collClients_m[COLL_IDX(kSynchronize)], _dist_client, msg);
        freeall_with(collClients_m[COLL_IDX(kSynchronize)], NULL);
        collClients_m[COLL_IDX(kSynchronize)] = NULL;

        spinunlock(&coll_lock_m);

        msg->release();
    }

    // wait for GO
    while ( false == isOK )
    {
        usleep(10);     // poll every .01 second
        spinlock(&coll_lock_m);
        isOK = collOK_m[COLL_IDX(kSynchronize)];
        spinunlock(&coll_lock_m);
    }

    return kCTChannelOK;
}



/*
 *              Runnable methods
 */

#define  _NO_OP         ;
#define CHK_CLIENT_STATUS(status, client, scmd) \
        if ( false == checkClientStatus(status, client) ) \
        { \
                client = NULL; \
                scmd; \
                ptr = NULL; \
                break; \
        }

void CTServer::performChecks(void *arg)
{
    link_t              *ptr;
    CTChannel   *chnl;
    char                *msg, flg, *ctrl;
    int                 clen;
    long int    mlen;
    long int    sz;
    CTChannelStatus     status;
    bool                hasN;

    while ( (true == isStarted_m) && (true == performingChecks_m) )
    {
        /* scan client connections for data.
           IMPORTANT:  since we always add at the beginning, then
           we should not conflict with adding new client connections.
        */
        spinlock(&lock_m);
        ptr = clients_m;
        spinunlock(&lock_m);
        while ( ptr && (true == isStarted_m) )
        {
            ctrl = NULL;
            chnl = (CTChannel *) ptr->data;
            if ( true == chnl->isReadable() )
            {
                /* msg layout should be:
                   <ctrl present flag (byte)>[<ctrl len (int)><ctrl info>]<packed msg>
                */
                //fprintf(stderr, "node [pid=%d]: receiving msg from client...\n", getpid());
                msg = NULL;
                flg = 0;
                status = chnl->receive(&flg, sizeof(flg), &sz);
                CHK_CLIENT_STATUS(status, ptr, _NO_OP);

                clen = 0;
                if ( flg && (kCTChannelOK == status) )
                {
                    status = chnl->receive((char *)&clen, sizeof(clen), &sz);
                    CHK_CLIENT_STATUS(status, ptr, _NO_OP);
                    if ( clen && (ctrl = (char *)malloc(sizeof(char)*clen))
                         && (kCTChannelOK == status) )
                    {
                        status = chnl->receive(ctrl, clen, &sz);
                        CHK_CLIENT_STATUS(status, ptr, free(ctrl));
                    }
                    else
                        ulm_err( ("Node %d: error in receiving ctrl field length. status = %d.\n",
                                  node_m->label(), status) );
                }

                if ( kCTChannelOK == status )
                    status = chnl->getPackedMessage(&msg, &mlen);
                if ( msg && (kCTChannelOK == status) )
                {
                    routeMessage(chnl, msg, mlen, clen, ctrl);
                    free(msg);
                }
                else
                {
                    if ( kCTChannelClosed != status )
                        ulm_err( ("Node %d: error in receiving msg. status = %d.\n",
                                  node_m->label(), status) );
                }

                free(ctrl);
            }
            ptr = ptr->next;
        }

        /* check heartbeats. */
        hasN = true;
        if ( (NULL  == node_m) || (false == node_m->hasNeighbors()) )
            hasN = false;

        /* verify that we need to continue checking. */

        if ( (NULL == clients_m) &&  (false == hasN) )
        {
            spinlock(&lock_m);
            performingChecks_m = false;
            spinunlock(&lock_m);
        }
        else
        {
            // sleep for awhile
            usleep(100);
        }
    }

    spinlock(&lock_m);
    performingChecks_m = false;
    spinunlock(&lock_m);

}


void CTServer::acceptConnections(void *arg)
{
    CTChannel           *client;
    link_t                      *item;
    CTChannelStatus     status;
    unsigned int        tag;
    long int            sent;

    while ( true == isStarted_m )
    {
        /* wait for client connections, then add to list of clients. */
        status = chserver_m->acceptConnections(0, &client);
        if ( kCTChannelOK != status )
        {
            // handle the error better!
            isStarted_m = false;
            continue;
        }

        if ( NULL == client )
        {
            /* check if we returned because server was told to stop. */
            if ( isStarted_m )
            {
                // how to handle error?
            }
        }
        else
        {
            item = newlink(client);
            spinlock(&lock_m);
            tag = netxtID_m++;
            client->setTag(tag);
            clients_m = addfront(clients_m,  item);
            spinunlock(&lock_m);

            // send clientID to client
            if ( kCTChannelOK == client->sendData((char *)&tag, sizeof(tag), &sent) )
            {
                spinlock(&lock_m);
                if ( false == performingChecks_m )
                {
                    performingChecks_m = true;
                    Thread::createThread(this, (runImpl_t)&CTServer::performChecks, NULL);
                }
                spinunlock(&lock_m);
            }
            else
            {
                // remove client from list.
            }
        }

    }
}


/*
 * Accessor Methods
 */

CTNode *CTServer::node() {return node_m;}
void CTServer::setNode(CTNode *node)
{
    delete node_m;
    node_m = node;
}
