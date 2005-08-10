/*
 * Copyright 2002-2005. The Regents of the University of
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

#ifndef GM_RECVFRAG
#define GM_RECVFRAG

#include "util/MemFunctions.h"
#include "internal/system.h"
#include "path/common/BaseDesc.h"        // needed for BaseRecvDesc_t
#include "path/gm/sendFrag.h"
#include "path/gm/state.h"
#include "path/gm/path.h"

// Debugging control
enum {
    DEBUG_INSERT_ARTIFICIAL_HEADER_CORRUPTION = 0,
    DEBUG_INSERT_ARTIFICIAL_DATA_CORRUPTION = 0
};


class gmRecvFragDesc : public BaseRecvFragDesc_t {
public:
    int dev_m;
    gmFragBuffer *gmFragBuffer_m;
    gmHeader *gmHeader_m;

    // copy function - specific to communications layer
    virtual unsigned int CopyFunction(void *fragAddr, void *appAddr,
                                      ssize_t length);

    // copy function for non-contiguous data with hooks for partial cksum word return
    virtual unsigned long nonContigCopyFunction(void *appAddr,
                                                void *fragAddr,
                                                ssize_t length,
                                                ssize_t cksumlength,
                                                unsigned int *cksum,
                                                unsigned int *partialInt,
                                                unsigned int *partialLength,
                                                bool firstCall,
                                                bool lastCall);

    // mark data as having passed or not the CheckData() call
    // without having to call CheckData() first
    virtual void MarkDataOK(bool okay)
        {
            DataOK = okay;
        }

    // check data
    virtual bool CheckData(unsigned int checkSum, ssize_t length);

    // return the data offset for this frag
    virtual unsigned long dataOffset()
        {
            return seqOffset_m;
        }

    // function to send acknowledgement
    virtual bool AckData(double timeNow = -1.0);

    // GM call back function to free resources for data ACKs
    static void ackCallback(struct gm_port *port,
                            void *context,
                            gm_status_t status);

    // return descriptor to descriptor pool
    virtual void ReturnDescToPool(int poolIndex)
        {
            if (gmFragBuffer_m) {
                if (usethreads()) {
                    gmState.localDevList[dev_m].Lock.lock();
                    gmState.localDevList[dev_m].bufList.returnElementNoLock(gmFragBuffer_m, 0);
                    gmState.localDevList[dev_m].Lock.unlock();
                }
                else
                    gmState.localDevList[dev_m].bufList.returnElementNoLock(gmFragBuffer_m, 0);
                /* reset gmFragBuffer_m to nil */
                gmFragBuffer_m = 0;
            }

            WhichQueue = GMRECVFRAGFREELIST;
            gmState.recvFragList.returnElement(this, 0);
        }


    // Initialize descriptor with data from fragment data header
    void msgData(double timeNow);

    // Free send resources on reception of a data ACK
    void msgDataAck(double timeNow);

    unsigned long long ackFragSequence()
    {
        gmHeaderDataAck     *p = &(gmHeader_m->dataAck);
        return p->thisFragSeq;
    }
    
    unsigned long long ackDeliveredFragSeq()
    {
        gmHeaderDataAck     *p = &(gmHeader_m->dataAck);
        return p->deliveredFragSeq;
    }
    
    unsigned long long ackReceivedFragSeq()
    {
        gmHeaderDataAck     *p = &(gmHeader_m->dataAck);
        return p->receivedFragSeq;
    }
    
    /* returns the global Proc ID of sending process */
    int ackSourceProc()
    {
        gmHeaderDataAck     *p = &(gmHeader_m->dataAck);
        return p->src_proc;
    }
    
    int ackStatus()
    {
        gmHeaderDataAck     *p = &(gmHeader_m->dataAck);
        return p->ackStatus;
    }
    
    // default constructor
    gmRecvFragDesc()
        {
            DataOK = false;
        }
    
    gmRecvFragDesc(int poolIndex):BaseRecvFragDesc_t(poolIndex)
        {
            DataOK = false;
        }
};

inline unsigned int gmRecvFragDesc::CopyFunction(void *fragAddr,
                                                 void *appAddr,
                                                 ssize_t length)
{
    if (!length) {
        return ((usecrc()) ? CRC_INITIAL_REGISTER : 0);
    }
    if (gmState.doChecksum) {
        if (usecrc()) {
            return bcopy_uicrc(fragAddr, appAddr, length, length_m);
        } else {
            return bcopy_uicsum(fragAddr, appAddr, length, length_m);
        }
    } else {
        MEMCOPY_FUNC(fragAddr, appAddr, length);
        return 0;
    }
}

// copy function for non-contiguous data with hooks for partial cksum
// word return

inline unsigned long
gmRecvFragDesc::nonContigCopyFunction(void *appAddr,
                                      void *fragAddr,
                                      ssize_t length,
                                      ssize_t cksumlength,
                                      unsigned int *cksum,
                                      unsigned int *partialInt,
                                      unsigned int *partialLength,
                                      bool firstCall,
                                      bool lastCall)
{
    if (!cksum || !gmState.doChecksum) {
        memcpy(appAddr, fragAddr, length);
    } else if (!usecrc()) {
        *cksum += bcopy_uicsum(fragAddr, appAddr, length, cksumlength,
                               partialInt, partialLength);
    } else {
        if (firstCall) {
            *cksum = CRC_INITIAL_REGISTER;
        }
        *cksum = bcopy_uicrc(fragAddr, appAddr, length, cksumlength, *cksum);
    }
    return length;
}

// check data
inline bool gmRecvFragDesc::CheckData(unsigned int checksum, ssize_t length)
{
    if (DEBUG_INSERT_ARTIFICIAL_DATA_CORRUPTION) {
        if ((rand() % 1021) == 0) {
            ulm_err(("Inserting artificial data corruption\n"));
            gmHeader_m->data.dataChecksum |= 0xA4A4;
        }
    }

    if (!length || !gmState.doChecksum) {
        DataOK = true;
    } else if (checksum == gmHeader_m->data.dataChecksum) {
        DataOK = true;
    } else {
        DataOK = false;
        ulm_err(("Warning: Corrupt fragment data received, rank %d --> rank %d: "
                 "(%s received=0x%x, calculated=0x%x)\n", 
                 gmHeader_m->data.senderID, myproc(),
                 (usecrc()) ? "CRC" : "checksum",
                 gmHeader_m->data.dataChecksum, checksum));
        if (!gmState.doAck) {
            ulm_exit(("Error: Reliability protocol not active. Cannot recover.\n"));
        }
    }
    return DataOK;
}

#endif
