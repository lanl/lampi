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
 *  CTController.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 02 2003.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "CTController.h"


CTChannelStatus CTController::sendControlField(unsigned int ctrlSize, char *control)
{
    return sendControlField(channel_m, ctrlSize, control);
}

CTChannelStatus CTController::sendControlField(CTChannel *chnl, unsigned int ctrlSize, char *control)
{
    char                        flg;
    long int            rlen;
    CTChannelStatus     status;

    /* msg layout should be:
       <ctrl present flag (byte)>[<ctrl len (int)><ctrl info>]<packed msg>
    */

    // send control info first.
    //send ctrl flag
    flg = (control) ? 1 : 0;
    status = chnl->sendData(&flg, sizeof(flg), &rlen);

    // send ctrl len
    if ( flg )
    {
        if ( kCTChannelOK == status )
            status = chnl->sendData((char *)&ctrlSize, sizeof(ctrlSize), &rlen);

        // send ctrl info
        if ( kCTChannelOK == status )
            status = chnl->sendData(control, ctrlSize, &rlen);
    }

    return status;
}


CTChannelStatus CTController::sendMessage(CTChannel *chnl, CTMessage *msg, unsigned int ctrlSize, char *control)
{
    CTChannelStatus             status;

    if ( kCTChannelOK == (status = sendControlField(chnl, ctrlSize, control)) )
    {
        status = chnl->sendMessage(msg);
    }
    return status;
}



CTChannelStatus CTController::broadcast(CTMessage *msg, unsigned int ctrlSize, char *control)
{
    CTChannelStatus     status = kCTChannelOK;

    /* msg layout should be:
       <ctrl present flag (byte)>[<ctrl len (int)><ctrl info>]<packed msg>
    */

    // send control info first.
    // send msg
    msg->setRoutingType(CTMessage::kBroadcast);
    if ( kCTChannelOK == sendControlField(ctrlSize, control) )
        status = channel_m->sendMessage(msg);

    return status;
}
