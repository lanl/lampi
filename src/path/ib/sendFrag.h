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

#ifndef IB_SENDFRAG_H
#define IB_SENDFRAG_H

#include "path/common/BaseDesc.h"
#undef PAGESIZE
#include "path/ib/state.h"

class ibSendFragDesc : public BaseSendFragDesc_t {
    public:
        enum qp_type {
            NOTASSIGNED_QP,
            UD_QP,
            RC_QP
        };

        enum state {
            UNINITIALIZED = 0,
            BASICINFO = (1 << 0),
            IBRESOURCES = (1 << 1),
            DATACOPIED = (1 << 2),
            INITCOMPLETE = (1 << 3),
            POSTED = (1 << 4),
            LOCALACKED = (1 << 5)
        };

        int type_m;
        int hca_index_m;
        int port_index_m;
        state state_m;
        qp_type qp_type_m;
        VAPI_sr_desc_t sr_desc_m;
        VAPI_sg_lst_entry_t sg_m[1];
        double timeSent_m;
        int globalDestID_m;
        unsigned long long frag_seq_m;
        void *reg_addr_m;
        bool inline_m;


        ibSendFragDesc(int poolIndex) { state_m = UNINITIALIZED; }

        unsigned int pack_buffer(void *dest);
        bool init(SendDesc_t *message, int hca, int port);
        bool init(enum ibCtlMsgTypes type, int glDestID, int hca, int port);
        bool init(void);
        bool ud_init(void);
        bool get_remote_ud_info(VAPI_ud_av_hndl_t *ah, VAPI_qp_num_t *qp);
        bool post(double timeNow, int *errorCode, bool already_locked);

        bool done(double timeNow, int *errorCode)
        {
            *errorCode = ULM_SUCCESS;
#ifdef ENABLE_RELIABILITY
            // put timeout logic in here...
            return ((state_m & LOCALACKED) != 0);
#else
            return ((state_m & LOCALACKED) != 0);
#endif
        }

        // return descriptor to fragment list, adjust tokens, etc.
        void free(bool already_locked = false)
        {
            bool locked_here = false;

            if (usethreads() && !already_locked) {
                ib_state.lock.lock();
                locked_here = true;
            }

            // reclaim tokens if we haven't already done so...
            if (((state_m & LOCALACKED) == 0) && ((state_m & POSTED) != 0)) {
                (ib_state.hca[hca_index_m].send_cq_tokens)++;
                (ib_state.hca[hca_index_m].ud.sq_tokens)++;
            }

            // clear all state except potentially whether IB resources 
            // are associated with this descriptor
            state_m = (state)(state_m & IBRESOURCES);

            (ib_state.hca[hca_index_m].send_frag_avail)++;
            WhichQueue = IBFRAGFREELIST;
            ib_state.hca[hca_index_m].send_frag_list.returnElementNoLock(this, 0);

            if (locked_here) {
                ib_state.lock.unlock();
            }
        }

};

#endif
