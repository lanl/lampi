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

#ifndef IB_PATH_H
#define IB_PATH_H

#include "ulm/ulm.h"
#include "util/dclock.h"
#include "path/common/path.h"
#undef PAGESIZE
#include "path/ib/state.h"

#if ENABLE_RELIABILITY
#include "internal/constants.h"
#endif

class ibPath : public BasePath_t {
    public:
        
        ibPath() { pathType_m = IBPATH; }

        ~ibPath() {}

        bool bind(SendDesc_t *message, int *globalDestProcessIDArray,
            int destArrayCount, int *errorCode) {
            if (isActive()) {
                *errorCode = ULM_SUCCESS;
                message->path_m = this;
                return true;
            }
            else {
                *errorCode = ULM_ERR_BAD_PATH;
                return false;
            }
        }

        void unbind(SendDesc_t *message, int *globalDestProcessIDArray,
            int destArrayCount) {
            ulm_exit((-1, "ibPath::unbind - called, not implemented yet...\n"));
        }

        bool init(SendDesc_t *message) {
            message->pathInfo.ib.allocated_offset_m = -1;
            message->pathInfo.ib.last_ack_time_m = 0.0;
            message->numfrags = 0;
            return true;
        }

        bool canReach(int globalDestProcessID);
        bool send(SendDesc_t *message, bool *incomplete, int *errorCode);
        bool sendDone(SendDesc_t *message, double timeNow, int *errorCode);
        bool receive(double timeNow, int *errorCode, recvType recvTypeArg);
        bool push(double timeNow, int *errorCode);
        bool needsPush(void);
        void finalize(void);
        void checkSendCQs(void);

#if ENABLE_RELIABILITY
        bool retransmitP(SendDesc_t *message) {
            if (!ib_state.ack || (ib_state.retrans_time == -1) || (message->earliestTimeToResend == -1)
                || (message->FragsToAck.size() == 0))
                return false;
            else if (dclock() >= message->earliestTimeToResend) {
                return true;
            } else {
                return false;
            }
        }

        bool resend(SendDesc_t *message, int *errorCode);
#endif

        bool sendCtlMsgs(int hca, double timeNow, int startIndex, int endIndex,
            int *errorCode, bool skipCheck = false, bool already_locked = false);

        // all HCAs version
        bool sendCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode,
            bool already_locked);

        bool cleanCtlMsgs(int hca, double timeNow, int startIndex, int endIndex,
            int *errorCode, bool skipCheck = false, bool already_locked = false);

        // all HCAs version
        bool cleanCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode,
            bool already_locked);
};
#endif
