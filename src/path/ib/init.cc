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

#include "path/ib/init.h"
#include "path/ib/state.h"
#include "internal/malloc.h"

void ibSetup(lampiState_t *s)
{
    VAPI_ret_t vapi_result;
    VAPI_qp_init_attr_t qpinit;
    IB_port_t p;
    int i, usable = 0;

    // get the number of available InfiniBand HCAs
    ib_state.num_hcas = 0;
    ib_state.num_active_hcas = 0;
    vapi_result = EVAPI_list_hcas(0, &ib_state.num_hcas, (VAPI_hca_id_t *)NULL);
    
    // no available HCAs...active_port is zero, exchange info now
    if (ib_state.num_hcas == 0) {
        goto exchange_info;
    }

    // only use first LAMPI_MAX_IB_HCAS HCAs
    if (ib_state.num_hcas > LAMPI_MAX_IB_HCAS) {
        ulm_warn(("%d InfiniBand HCAs available, LAMPI_MAX_IB_HCAS set to %d\n",
            ib_state.num_hcas, LAMPI_MAX_IB_HCAS));
        ib_state.num_hcas = LAMPI_MAX_IB_HCAS;
    }

    // get ids
    vapi_result = EVAPI_list_hcas(
        sizeof(VAPI_hca_id_t) * ib_state.num_hcas,
        &(ib_state.num_hcas),
        ib_state.hca_ids);
    if (vapi_result != VAPI_OK) {
        ulm_err(("ibSetup: EVAPI_list_hcas() returned %s\n", 
            VAPI_strerror(vapi_result)));
        exit(1);
    }

    // get handles and initialize port info array to zero...
    for (i = 0; i < (int)ib_state.num_hcas; i++) {
        vapi_result = EVAPI_get_hca_hndl(ib_state.hca_ids[i], &(ib_state.hca[i].handle));
        if (vapi_result == VAPI_OK) {
            ib_state.hca[i].usable = true;
            usable++;
        }
        else {
            ib_state.hca[i].usable = false;
        }
        ib_state.hca[i].num_active_ports = 0;
    }

    if (usable == 0) {
        goto exchange_info;
    }

    // get HCA capabilities and vendor info...allocate port cap arrays and get port info
    for (i = 0; i < (int)ib_state.num_hcas; i++) {
        bool hca_active = false;
        if (ib_state.hca[i].usable) {
            vapi_result = VAPI_query_hca_cap(
                ib_state.hca[i].handle, 
                &(ib_state.hca[i].vendor),
                &(ib_state.hca[i].cap));
            if (vapi_result != VAPI_OK) {
                ulm_err(("ibSetup: VAPI_query_hca_cap() for HCA %d returned %s\n", 
                    i, VAPI_strerror(vapi_result)));
                    exit(1);
            }

            // only use first LAMPI_MAX_IB_HCA_PORTS of an HCA
            IB_port_t num_ports = (ib_state.hca[i].cap.phys_port_num > (IB_port_t)LAMPI_MAX_IB_HCA_PORTS) ?
                (IB_port_t)LAMPI_MAX_IB_HCA_PORTS : ib_state.hca[i].cap.phys_port_num;

            // get the port capabilities
            for (p = 0; p < num_ports; p++) {
                // VAPI starts port indices at 1
                vapi_result = VAPI_query_hca_port_prop(ib_state.hca[i].handle, (p + 1),
                    &(ib_state.hca[i].ports[p]));
                if (vapi_result != VAPI_OK) {
                    ulm_err(("ibSetup: VAPI_query_hca_port_cap() for HCA %d port %d returned %s\n", 
                        i, (p + 1), VAPI_strerror(vapi_result)));
                    exit(1);
                }
                // count the port if it's active and has its MTU set to at least 2KB
                if ((ib_state.hca[i].ports[p].state == PORT_ACTIVE) && 
                    (ib_state.hca[i].ports[p].max_mtu >= MTU2048)) {
                    ib_state.hca[i].active_ports[ib_state.hca[i].num_active_ports++] = p;
                    hca_active = true;
                }
            }
            // set the HCA as active if there is at least one active port
            if (hca_active) {
                ib_state.active_hcas[ib_state.num_active_hcas++] = i;
            }
        }
    } 
    
    if (ib_state.num_active_hcas == 0) {
        goto exchange_info;
    }

    for (i = 0; i < ib_state.num_active_hcas; i++) {
        ib_hca_state_t *h = &(ib_state.hca[ib_state.active_hcas[i]]);
        // allocate protection domain for HCA
        vapi_result = VAPI_alloc_pd(h->handle, &(h->pd));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_alloc_pd() for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // allocate receive completion queue for HCA
        vapi_result = VAPI_create_cq(h->handle, h->cap.max_num_ent_cq,
            &(h->recv_cq), &(h->recv_cq_size));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_cq() [recv] for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // allocate send completion queue for HCA
        vapi_result = VAPI_create_cq(h->handle, h->cap.max_num_ent_cq,
            &(h->send_cq), &(h->send_cq_size));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_cq() [send] for HCA %d returned %s\n", 
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
        // allocate a single UD QP
        qpinit.sq_cq_hndl = h->send_cq;
        qpinit.rq_cq_hndl = h->recv_cq;
        qpinit.cap.max_oust_wr_sq = h->cap.max_qp_ous_wr;
        qpinit.cap.max_oust_wr_rq = h->cap.max_qp_ous_wr;
        qpinit.cap.max_sg_size_sq = h->cap.max_num_sg_ent;
        qpinit.cap.max_sg_size_rq = h->cap.max_num_sg_ent;
        qpinit.sq_sig_type = VAPI_SIGNAL_ALL_WR;
        qpinit.rq_sig_type = VAPI_SIGNAL_ALL_WR;
        qpinit.pd_hndl = h->pd;
        qpinit.ts_type = VAPI_TS_UD;
        vapi_result = VAPI_create_qp(h->handle, &qpinit, &(h->ud.handle), &(h->ud.prop));
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: VAPI_create_qp() for HCA %d returned %s\n",
                ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            exit(1);
        }
    }

exchange_info:

    return;
}

