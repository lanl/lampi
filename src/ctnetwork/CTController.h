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
 *  CTController.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#ifndef _ADMIN_CONTROLLER_H_
#define _ADMIN_CONTROLLER_H_

#include "internal/MPIIncludes.h"
#include "ctnetwork/CTChannel.h"
#include "ctnetwork/CTDelegate.h"
#include "ctnetwork/CTMessage.h"
#include "threads/Runnable.h"

// Error reporting

class CTController : public Runnable
{
    /*
     * Instance Variables
     */
         
protected:
    CTChannel               *channel_m;
    CTDelegate              *delegate_m;
    int                             blksize_m;              /* # of nodes per block when sending messages to large # of nodes. */
        
    /*
     * Instance Methods
     */
         
public:
    CTController() : channel_m(NULL), delegate_m(NULL), blksize_m(10) {}
    
    virtual CTChannelStatus sendControlField(unsigned int ctrlSize, char *control);
    virtual CTChannelStatus sendControlField(CTChannel *chnl, unsigned int ctrlSize, char *control);
        
    virtual CTChannelStatus sendMessage(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *control);
        
    virtual CTChannelStatus broadcast(CTMessage *msg, unsigned int ctrlSize, char *control);
        
    virtual CTChannelStatus allgather(unsigned int numParticipants, unsigned int datalen, 
                                      char *sendBuf, char *recvBuf) = 0;
    /*
      POST:   Gathers individual node data to node 0 then broadcasts aggregate data to all nodes
      and participating CTClients. recvBuf +i*datalen is data from node i and length datalen.
    */
        
        
        
    virtual CTChannelStatus scatterv(int msgType, unsigned int *datalen, const char **data) = 0;
    /*
      PRE:    The number of items in array should equal the number of nodes in the network.
      data is an array of msg data to send to specific nodes.
      data[i] will be sent to node with label i.
      datalen[i] is length of data[i] in bytes.
      POST:   Distributes data to network nodes so that node i receives data[i].
      NOTE: This does not account for CTClients that are attached to the nodes.
    */
        
    /*
      virtual void scatter() = 0;
        
      virtual void gather() = 0;
      virtual void gatherv() = 0;
    */
        
    virtual CTChannelStatus synchronize(unsigned int numParticipants) = 0;
    /* barrier-like method. */
        
    /*
     * Accessor Methods
     */
        
    CTChannel       *channel() {return channel_m;}
        
    int blockSize() {return blksize_m;}
    void setBlockSize(int sz) {blksize_m = sz;}
        
    CTDelegate *delegate() {return delegate_m;}
    void setDelegate(CTDelegate *delegate) {delegate_m = delegate;}
};

#endif
