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
 *  CTMessage.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Mon Jan 13 2003.
 */

#include <strings.h>

#include "CTMessage.h"
#include "internal/malloc.h"
#include "internal/log.h"

typedef struct
{
    unsigned int    _destination;   /* label of destination node for pt-to-pt. */
    unsigned int    _destinationID; /* client ID of destination CTClient. */
    unsigned int    _sourceNode;    /* node from which msg originated. */
    unsigned int    _sendingNode;   /* node that had just relayed msg. */
    unsigned int    _clientID;              /* if greater than 0, then msg is sent from client (as opposed to server node). */
} msg_control_t;


static char *_pack_ctrl(msg_control_t *ctrl, long int *len)
{
    /*
      packed layout is:
      _destination : _destinationID : _sourceNode : _sendingNode : _clientID
    */
    int     sz, tmp;
    char    *buf;

    if ( NULL == ctrl ) return NULL;

    sz  = sizeof(ctrl->_destination) +
        sizeof(ctrl->_destinationID) +
        sizeof(ctrl->_sourceNode) +
        sizeof(ctrl->_sendingNode) +
        sizeof(ctrl->_clientID);

    if ( NULL != (buf = (char *)ulm_malloc(sz)) )
    {
        *len = 0;
        memcpy(buf + (*len), &(ctrl->_destination), tmp = sizeof(ctrl->_destination));
        *len += tmp;
        memcpy(buf + (*len), &(ctrl->_destinationID), tmp = sizeof(ctrl->_destinationID));
        *len += tmp;
        memcpy(buf + (*len), &(ctrl->_sourceNode), tmp = sizeof(ctrl->_sourceNode));
        *len += tmp;
        memcpy(buf + (*len), &(ctrl->_sendingNode), tmp = sizeof(ctrl->_sendingNode));
        *len += tmp;
        memcpy(buf + (*len), &(ctrl->_clientID), tmp = sizeof(ctrl->_clientID));
        *len += tmp;
    }
    return buf;
}

static msg_control_t *_unpack_ctrl(const char *buf, int *clen)
{
    /*
      packed layout is:
      _destination : _destinationID : _sourceNode : _sendingNode : _clientID
    */
    msg_control_t   *ctrl;
    int                             sz;
    const char              *ptr;

    *clen = 0;
    if ( NULL == (ctrl = (msg_control_t *)ulm_malloc(sizeof(msg_control_t)))  )
        return NULL;

    ptr = buf;
    memcpy(&(ctrl->_destination), ptr, sz = sizeof(ctrl->_destination));
    ptr += sz;
    memcpy(&(ctrl->_destinationID), ptr, sz = sizeof(ctrl->_destinationID));
    ptr += sz;
    memcpy(&(ctrl->_sourceNode), ptr, sz = sizeof(ctrl->_sourceNode));
    ptr += sz;
    memcpy(&(ctrl->_sendingNode), ptr, sz = sizeof(ctrl->_sendingNode));
    ptr += sz;
    memcpy(&(ctrl->_clientID), ptr, sz = sizeof(ctrl->_clientID));
    ptr += sz;

    if ( ctrl )
        *clen = ptr - buf;

    return ctrl;
}


static const char *_skip_control(const char *ptrToControl)
{
    /*
      packed layout is:
      _destination : _destinationID : _sourceNode : _sendingNode : _clientID
    */
    // skip _destination : _destinationID : _sourceNode : _sendingNode : _clientID
    ptrToControl += 5*sizeof(unsigned int);         

    return ptrToControl;
}

/*
 *
 *                      CTMessage  Implementation
 *
 */


CTMessage::CTMessage(int type, const char *data, long int datalen, bool copyData)
    : type_m(type), rtype_m(kPointToPoint), netcmd_m(0),
      control_m(NULL), data_m(NULL), datalen_m(datalen), copy_m(copyData)
{
    control_m = (msg_control_t *)ulm_malloc(sizeof(msg_control_t));
    bzero(control_m, sizeof(msg_control_t));

    if ( copy_m )
    {
        if ( datalen )
        {
            if ( (data_m = (char *)ulm_malloc(sizeof(char)*datalen)) )
                memcpy(data_m, data, datalen);
            else
            {
                datalen = 0;
                ulm_err(("Error: Unable to alloc memory for msg data.\n"));
            }            
        }
    }
    else
        data_m = (char *)data;
}

CTMessage::~CTMessage()
{
    if ( copy_m )
        ulm_free2(data_m);
    ulm_free2(control_m);
}


const char *CTMessage::getData(const char *buffer)
{
    /*
      PRE:    packed layout:
      type (char) : routing type (char) : net cmd (char) : packed cntrl : datalen (long int) : data
      packed cntrl:
      destination : sending node : path length : path
    */
    const char              *ptr;

    if ( !buffer ) return NULL;

    // skip to packed control struct
    ptr = buffer + 3*sizeof(char);
    ptr = _skip_control(ptr);
    ptr += sizeof(long int);                // datalen field

    return ptr;
}



int CTMessage::getMessageType(const char *buffer)
{
    char            tp;

    memcpy(&tp, buffer, sizeof(tp));
    return (int)tp;
}


int CTMessage::getRoutingType(const char *buffer)
{
    char            tp;

    memcpy(&tp, buffer+sizeof(char), sizeof(tp));
    return (int)tp;
}

int CTMessage::getNetworkCommand(const char *buffer)
{
    char            tp;

    memcpy(&tp, buffer+2*sizeof(char), sizeof(tp));
    return (int)tp;
}


bool CTMessage::getDestination(char *packedMsg, unsigned int *dst)
{
    char            tp;
    bool            found = false;

    memcpy(&tp, packedMsg+sizeof(char), sizeof(tp));
    if ( kPointToPoint == tp )
    {
        // there is only a destination if pt-to-pt
        //  skip over type (char) : routing type (char) : net cmd (char)
        memcpy(dst, packedMsg + 3*sizeof(char), sizeof(unsigned int));
        found = true;
    }
    return found;
}

unsigned int CTMessage::getSendingNode(char *packedMsg)
{
    /*
      PRE:       packed layout:
      type (char) : routing type (char) : net cmd (char) : packed cntrl : datalen (long int) : data
      packed cntrl:
      destination : _destinationID : source node : sending node : path length : path
    */
    char        *ctrl;

    if ( NULL == packedMsg ) return 0;
    ctrl = packedMsg + 3*sizeof(char);
    ctrl += 3*sizeof(unsigned int);
    return *(unsigned int *)ctrl;
}

void CTMessage::setSendingNode(char *packedMsg, unsigned int label)
{
    /*
      PRE:    packed layout:
      type (char) : routing type (char) : net cmd (char) : packed cntrl : datalen (long int) : data
      packed cntrl:
      destination : _destinationID : source node : sending node : path length : path
    */
    char    *ctrl;

    if ( NULL == packedMsg ) return;
    ctrl = packedMsg + 3*sizeof(char);
    ctrl += 3*sizeof(unsigned int);
    memcpy(ctrl, &label, sizeof(unsigned int));
}


unsigned int CTMessage::getSourceNode(char *packedMsg)
{
    /*
      PRE:    packed layout:
      type (char) : routing type (char) : net cmd (char) : packed cntrl : datalen (long int) : data
      packed cntrl:
      destination : _destinationID : source node : sending node : path length : path
    */
    char    *ctrl;

    if ( NULL == packedMsg ) return 0;
    ctrl = packedMsg + 3*sizeof(char);
    ctrl += 2*sizeof(unsigned int);
    return *(unsigned int *)ctrl;
}



CTMessage *CTMessage::unpack(const char *buffer)
{
    /*
      PRE:    packed layout:
      type (char) : routing type (char) : packed cntrl : datalen (long int) : data
    */
    char            rtp, mtp, cmd;
    CTMessage       *msg;
    int                     sz;
    long int        datalen;
    msg_control_t   *ctrl;
    const char *ptr;
    char    *data;

    ctrl = NULL;
    data = NULL;
    ptr = buffer;
    memcpy(&mtp, ptr, sz = sizeof(mtp));
    ptr += sz;
    memcpy(&rtp, ptr, sz = sizeof(rtp));
    ptr += sz;
    memcpy(&cmd, ptr, sz = sizeof(cmd));
    ptr += sz;

    ctrl = _unpack_ctrl(ptr,  &sz);
    ptr += sz;

    memcpy(&datalen, ptr, sz = sizeof(datalen));
    ptr += sz;

    if ( datalen > 100000 )
        ulm_ferr(("Warning: Unpacking msg with size > 100000: src = %d, dest = %d.\n",
                  ctrl->_sourceNode, ctrl->_destination));

    if ( datalen )
    {
        if ( (data = (char *)ulm_malloc(sizeof(char)*datalen)) )
        {
            memcpy(data, ptr, datalen);
        }        
    }

    /*
      len = (ptr-buffer)+datalen;
      for ( int i = 0; i < len; i++ )
      fprintf(stderr, "%x", *(buffer+i));
      fprintf(stderr, "\n");
    */

    msg = new CTMessage(mtp, NULL, 0);
    msg->netcmd_m = cmd;
    msg->rtype_m = rtp;
    msg->control_m = ctrl;
    msg->data_m = data;
    msg->datalen_m = datalen;
        
    return msg;
}



bool CTMessage::pack(char **buffer, long int *len)
{
    /*
      POST:   packs msg into a byte stream for transmission.  caller is
      responsible for freeing buffer.  *len contains byte count.
      packed layout:
      type (char) : routing type (char) : net cmd (char) : packed cntrl : datalen (long int) : data
    */
    char            *ctrl, *ptr;
    long int        clen;
    bool            success = true;

    *buffer = NULL;
    *len = 0;
    clen = 0;
    ctrl = _pack_ctrl((msg_control_t*)control_m, &clen);

    *len += ( clen + sizeof(type_m) + sizeof(rtype_m) + sizeof(netcmd_m)
              + sizeof(datalen_m) + datalen_m );
    if ( NULL == (*buffer = (char *)ulm_malloc(sizeof(char)*(*len))) )
    {
        *len = 0;
        ulm_free2(ctrl);
        return false;
    }

    ptr = *buffer;
    memcpy(ptr, &type_m, sizeof(type_m));           /* msg type. */
    ptr += sizeof(type_m);
    memcpy(ptr, &rtype_m, sizeof(rtype_m));         /* routing type. */
    ptr += sizeof(rtype_m);
    memcpy(ptr, &netcmd_m, sizeof(netcmd_m));       /* net cmd. */
    ptr += sizeof(netcmd_m);
    if ( ctrl )
    {
        memcpy(ptr, ctrl, clen);                    /* control struct. */
        ptr += clen;
    }
    memcpy(ptr, &datalen_m, sizeof(datalen_m));     /* data len. */
    ptr += sizeof(datalen_m);
    memcpy(ptr, data_m, datalen_m);                 /* user data. */
    ptr += datalen_m;

    /*
      for ( int i = 0; i < *len; i++ )
      fprintf(stderr, "%x", *((*buffer)+i));
      fprintf(stderr, "\n");
    */

    ulm_free2(ctrl);

    return success;
}



unsigned int CTMessage::destination()
{
    if (control_m)
        return ((msg_control_t*)control_m)->_destination;
    else
        return (unsigned int)NTMSG_INVALID_DEST;
}

void CTMessage::setDestination(unsigned int label)
{
    if (control_m)
        ((msg_control_t *)control_m)->_destination = label;
}

unsigned int CTMessage::destinationClientID()
{
    if (control_m)
        return ((msg_control_t*)control_m)->_destinationID;
    else
        return (unsigned int)NTMSG_INVALID_DEST;
}

void CTMessage::setDestinationClientID(unsigned int clientID)
{
    if (control_m)
        ((msg_control_t *)control_m)->_destinationID = clientID;
}

unsigned int CTMessage::sendingClientID()
{
    return ((msg_control_t *)control_m)->_clientID;
}


void CTMessage::setSendingClientID(unsigned int clientID)
{
    ((msg_control_t *)control_m)->_clientID = clientID;
}



unsigned int CTMessage::sendingNode()
{
    return ((msg_control_t *)control_m)->_sendingNode;
}

void CTMessage::setSendingNode(unsigned int label)
{
    ((msg_control_t *)control_m)->_sendingNode = label;
}

unsigned int CTMessage::sourceNode()
{
    return ((msg_control_t *)control_m)->_sourceNode;
}

void CTMessage::setSourceNode(unsigned int label)
{
    ((msg_control_t *)control_m)->_sourceNode = label;
}
