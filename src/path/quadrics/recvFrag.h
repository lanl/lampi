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

#ifndef QUADRICS_RECVDESC
#define QUADRICS_RECVDESC

#include <elan3/elan3.h>

#include "util/MemFunctions.h"
#include "internal/system.h"
#include "path/common/BaseDesc.h"              // needed for BaseRecvDesc_t
#include "path/quadrics/header.h"
#include "path/quadrics/state.h"

// forward declarations
class quadricsPath;
class quadricsSendFragDesc;

class quadricsRecvFragDesc : public BaseRecvFragDesc_t {
public:
    quadricsCtlHdr_t envelope;
    bool DataOK;
    ELAN3_CTX *ctx;
    int rail;
    quadricsPath *path;

    // copy function - specific to communications layer
    virtual unsigned int CopyFunction(void *fragAddr, void *appAddr,
                                      ssize_t length)
        {
            if (!length) {
                return ((usecrc()) ? CRC_INITIAL_REGISTER : 0);
            }
            if (quadricsDoChecksum) {
                if (usecrc()) {
                    return bcopy_uicrc(fragAddr, appAddr, length, length_m);
                }
                else {
                    return bcopy_uicsum(fragAddr, appAddr, length, length_m);
                }
            }
            else {
                MEMCOPY_FUNC(fragAddr, appAddr, length);
                return 0;
            }
        }

    // copy function for non-contiguous data with hooks for partial cksum word return
    virtual unsigned long nonContigCopyFunction(void *appAddr,
                                                void *fragAddr,
                                                ssize_t length,
                                                ssize_t cksumlength,
                                                unsigned int *cksum,
                                                unsigned int *partialInt,
                                                unsigned int *partialLength,
                                                bool firstCall,
                                                bool lastCall)
        {
            if (!cksum || !quadricsDoChecksum) {
                memcpy(appAddr, fragAddr, length);
            }
            else if (!usecrc()) {
                *cksum += bcopy_uicsum(fragAddr, appAddr, length, cksumlength,
                                       partialInt, partialLength);
            }
            else {
                if (firstCall) {
                    *cksum = CRC_INITIAL_REGISTER;
                }
                *cksum = bcopy_uicrc(fragAddr, appAddr, length, cksumlength, *cksum);
            }
            return length;
        }

    // mark data as having passed or not the CheckData() call
    // without having to call CheckData() first
    virtual void MarkDataOK(bool okay)
        {
            DataOK = okay;
        }

    // check data
    virtual bool CheckData(unsigned int checkSum, ssize_t length)
        {
            if (!length || !quadricsDoChecksum) {
                DataOK = true;
            }
            else if (checkSum == envelope.msgDataHdr.dataChecksum) {
                DataOK = true;
            }
            else {
                DataOK = false;
                if (!quadricsDoAck) {
                    ulm_exit((-1, "quadricsRecvFragDesc::CheckData: - corrupt data received "
                              "(%s 0x%x calculated 0x%x)\n", (usecrc()) ? "CRC" : "checksum",
                              envelope.msgDataHdr.dataChecksum, checkSum));
                }
            }
            return DataOK;
        }


    // return the data offset for this frag
    virtual unsigned long dataOffset()
        {
            return (unsigned long)envelope.msgDataHdr.dataSeqOffset;
        }

    // function to send acknowledgement
    virtual bool AckData(double timeNow = -1.0);

    // return descriptor to descriptor pool
    virtual void ReturnDescToPool(int poolIndex)
        {
            WhichQueue = QUADRICSRECVFRAGFREELIST;
            quadricsRecvFragDescs.returnElement(this, poolIndex);
        }

    // default constructor
    quadricsRecvFragDesc() { DataOK = false; }
    quadricsRecvFragDesc(int poolIndex) : BaseRecvFragDesc_t(poolIndex) { DataOK = false; }

    // destructor
    ~quadricsRecvFragDesc() {}

#ifdef RELIABILITY_ON
    // check to see if multicast frag is a duplicate
    bool isDuplicateCollectiveFrag();

    // check to see if an ACK is a duplicate or non-specific only
    bool checkForDuplicateAndNonSpecificAck(quadricsSendFragDesc *sfd);
#endif

    void handlePt2PtMessageAck(double timeNow, BaseSendDesc_t *bsd,
                               quadricsSendFragDesc *sfd);

    // process the received data as...
    void msgData(double timeNow);
    void msgDataAck(double timeNow);
    void memRel();
    void memReq(double timeNow);
    void memReqAck();
};

#endif
