/*
 * Copyright 2002-2003.  The Regents of the University of
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

#ifndef IB_RECVFRAG_H
#define IB_RECVFRAG_H

#include "path/common/BaseDesc.h"
#undef PAGESIZE
#include "path/ib/path.h"
#include "path/ib/state.h"
#include "path/ib/header.h"

class ibRecvFragDesc : public BaseRecvFragDesc_t {
    public:

        enum qp_type {
            UD_QP,
            OTHER_QP
        };

        int hca_index_m;
        unsigned long ib_bytes_recvd_m;
        qp_type qp_type_m;
        VAPI_rr_desc_t rr_desc_m;
        VAPI_sg_lst_entry_t sg_m[1];
        ibData2KMsg_t *msg_m;
        ibPath *path;


        ibRecvFragDesc() { }
        ibRecvFragDesc(int poolIndex): BaseRecvFragDesc_t(poolIndex) { }

        void msgData(double timeNow);
        void msgDataAck(double timeNow);
        void handlePt2PtMessageAck(double timeNow, SendDesc_t *bsd, 
            ibSendFragDesc *sfd, bool already_locked);

#ifdef ENABLE_RELIABILITY
        bool checkForDuplicateAndNonSpecificAck(ibSendFragDesc *sfd, double timeNow);
#endif

        bool AckData(double timeNow);
        unsigned int CopyFunction(void *fragAddr, void *appAddr, ssize_t length);
        unsigned long nonContigCopyFunction(void *appAddr, void *fragAddr, ssize_t length,
            ssize_t cksumlength, unsigned int *cksum, unsigned int *partialInt, 
            unsigned int *partialLength, bool firstCall, bool lastCall);
        bool CheckData(unsigned int checkSum, ssize_t length);

        void MarkDataOK(bool okay) {
            DataOK = okay;
        }

        unsigned long dataOffset(void) {
            return (unsigned long)(msg_m->header.dataSeqOffset);
        }

        void ReturnDescToPool(int LocalRank);
};

#endif
