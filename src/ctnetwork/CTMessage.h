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
 *  CTMessage.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Mon Jan 13 2003.
 */

#ifndef _ADMIN_MESSAGE_H_
#define _ADMIN_MESSAGE_H_

#include "internal/MPIIncludes.h"
#include "util/RCObject.h"

/* ASSERT: all destinations are nonnegative. */
#define NTMSG_INVALID_DEST		-1

class CTMessage:public RCObject {
    /*
     *              Instance Variables Declarations
     */
protected:
    char type_m;                /* USER, NETWORK */
    char rtype_m;               /* bcast, pt-to-pt, scatter, etc. */
    char netcmd_m;              /* network admin command. */
    void *control_m;
    char *data_m;
    long int datalen_m;
    bool copy_m;

public:
    enum {
        kNetwork = 0,
        kUser
    };                          /*msg type */


    enum {
        kLinkup = 0,            /* Should only be sent to link up nodes in network. */
        kBroadcast,
        kScatter,
        kScatterv,
        kLocal,                 /*      meant for sending messages by CTClient to a server that CTClient 
                                        is directly connected to. */
        kPointToPoint
    };                          /* routing type. */

    /*
     *              Instance Methods
     */

private:

public:
    CTMessage(int type, const char *data, long int datalen,
              bool copyData = true);
    ~CTMessage();

    /*
     * Helper class methods for processing packed msgs.
     */

    static const char *getData(const char *buffer);
    /*
      returns pointer to location of message data in packed msg.
    */

    static bool getDestination(char *packedMsg, unsigned int *dst);

    static int getMessageType(const char *buffer);
    /*
      Used to make routing more efficient by allowing caller to check msg
      type without having to unpack entire msg.
    */

    static int getRoutingType(const char *buffer);
    /*
      Used to make routing more efficient by allowing caller to check routing
      type without having to unpack entire msg.
    */

    static int getNetworkCommand(const char *buffer);
    /*
      Used to make routing more efficient by allowing caller to check routing
      type without having to unpack entire msg.
    */

    static void setSendingNode(char *packedMsg, unsigned int label);

    static unsigned int getSendingNode(char *packedMsg);
    static unsigned int getSourceNode(char *packedMsg);

    static CTMessage *unpack(const char *buffer);


    /*
     *              Instance methods
     */

    bool pack(char **buffer, long int *len);
    /*
      POST:        packs msg into a byte stream for transmission.  caller is
      responsible for freeing buffer.  *len contains byte count.
    */

    /*
     *              Accessor Methods
     */

    char *data() {
        return data_m;
    } long int dataLength() {
        return datalen_m;
    }

    unsigned int sendingClientID();
    void setSendingClientID(unsigned int clientID);

    // currently, sendingNode is only meaningful during initial network linkup.  Fix to get rid of this!
    unsigned int sendingNode();
    void setSendingNode(unsigned int label);

    unsigned int sourceNode();
    void setSourceNode(unsigned int label);

    unsigned int destination();
    void setDestination(unsigned int label);

    unsigned int destinationClientID();
    void setDestinationClientID(unsigned int clientID);

    char networkCommand() {
        return netcmd_m;
    }
    void setNetworkCommand(char cmd) {
        netcmd_m = cmd;
    }

    int type() {
        return type_m;
    }
    int routingType() {
        return rtype_m;
    }
    void setRoutingType(int type) {
        rtype_m = type;
    }
};


#endif
