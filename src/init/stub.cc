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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * device specific functions default to noop if they are not defined
 */

#include "init/init.h"

static void noop(lampiState_t *s)
{
    return;
}

#if ENABLE_RMS == 0
lampi_init_func_t lampi_init_prefork_rms = noop;
lampi_init_func_t lampi_init_postfork_rms = noop;
lampi_init_func_t lampi_init_allforked_rms = noop;
#endif

#if ENABLE_GM == 0
lampi_init_func_t lampi_init_prefork_gm = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_msg_gm = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_params_gm = noop;
lampi_init_func_t lampi_init_postfork_gm = noop;
#endif

#if ENABLE_QSNET == 0
lampi_init_func_t lampi_init_prefork_quadrics = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_msg_quadrics = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_params_quadrics = noop;
lampi_init_func_t lampi_init_postfork_quadrics = noop;
lampi_init_func_t lampi_init_allforked_quadrics = noop;
#ifdef USE_ELAN_COLL
lampi_init_func_t lampi_init_postfork_coll_setup = noop;
#endif
#endif

#if ENABLE_SHARED_MEMORY == 0
lampi_init_func_t lampi_init_prefork_receive_setup_msg_shared_memory = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_params_shared_memory = noop;
lampi_init_func_t lampi_init_prefork_shared_memory = noop;
lampi_init_func_t lampi_init_postfork_shared_memory = noop;
lampi_init_func_t lampi_init_allforked_local_coll = noop;
lampi_init_func_t lampi_init_allforked_shared_memory = noop;
#endif

#if ENABLE_UDP == 0
lampi_init_func_t lampi_init_prefork_udp = noop;
lampi_init_func_t lampi_init_postfork_udp = noop;
lampi_init_func_t lampi_init_allforked_udp = noop;
#endif

#if ENABLE_TCP == 0
lampi_init_func_t lampi_init_prefork_tcp = noop;
lampi_init_func_t lampi_init_postfork_tcp = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_params_tcp = noop;
#endif

#if ENABLE_INFINIBAND == 0
lampi_init_func_t lampi_init_prefork_receive_setup_msg_ib = noop;
lampi_init_func_t lampi_init_prefork_receive_setup_params_ib = noop;
lampi_init_func_t lampi_init_postfork_ib = noop;
#endif
