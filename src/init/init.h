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


#ifndef _LAMPI_INIT_H_
#define _LAMPI_INIT_H_

#include <stdio.h>
#include <sys/types.h>

#include "client/adminMessage.h"

/*
 * Configuration macros
 */

#ifdef __osf__
#define USE_RMS
#endif

#include "internal/state.h"


/*
 * error states during initialization
 */
enum {
    LAMPI_INIT_SUCCESS = 0,
    ERROR_LAMPI_INIT_PREFORK,
    ERROR_LAMPI_INIT_AT_FORK,
    ERROR_LAMPI_INIT_POSTFORK,
    ERROR_LAMPI_INIT_ALLFORKED,
    ERROR_LAMPI_INIT_PREFORK_INITIALIZING_GLOBALS,
    ERROR_LAMPI_INIT_PREFORK_INITIALIZING_PROCESS_RESOURCES,
    ERROR_LAMPI_INIT_PREFORK_INITIALIZING_STATE_INFORMATION,
    ERROR_LAMPI_INIT_SETTING_UP_CORE,
    ERROR_LAMPI_INIT_PREFORK_RESOURCE_MANAGEMENT,
    ERROR_LAMPI_INIT_POSTFORK_RESOURCE_MANAGEMENT,
    ERROR_LAMPI_INIT_ALLFORKED_RESOURCE_MANAGEMENT,
    ERROR_LAMPI_INIT_CONNECT_TO_DAEMON,
    ERROR_LAMPI_INIT_CONNECT_TO_MPIRUN,
    ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS,
    ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY,
    ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS,
    ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_GM,
    ERROR_LAMPI_INIT_PREFORK_PATHS,
    ERROR_LAMPI_INIT_POSTFORK_PATHS,
    ERROR_LAMPI_INIT_ALLFORKED_PATHS,
    ERROR_LAMPI_INIT_PREFORK_UDP,
    ERROR_LAMPI_INIT_POSTFORK_UDP,
    ERROR_LAMPI_INIT_PREFORK_RMS,
    ERROR_LAMPI_INIT_POSTFORK_RMS,
    ERROR_LAMPI_INIT_POSTFORK_GM,
    ERROR_LAMPI_INIT_PATH_CONTAINER,
    ERROR_LAMPI_INIT_WAIT_FOR_START_MESSAGE,
    ERROR_LAMPI_INIT_MAX
};


/*
 * inline functions
 */
inline static void lampi_init_print(const char *string)
{
    fprintf(stderr, "%s\n", string);
    fflush(stderr);
}

/*
 * prototypes
 */
void lampi_init(void);


/*
 * prototypes for initialization phases
 */
void lampi_init_prefork_connect_to_daemon(lampiState_t *);
void lampi_init_prefork_connect_to_mpirun(lampiState_t *);
void lampi_init_prefork_debugger(lampiState_t *);
void lampi_init_prefork_environment(lampiState_t *);
void lampi_init_prefork_globals(lampiState_t *);
void lampi_init_prefork_initialize_state_information(lampiState_t *);
void lampi_init_prefork_parse_setup_data(lampiState_t *);
void lampi_init_prefork_paths(lampiState_t *);
void lampi_init_prefork_process_resources(lampiState_t *);
void lampi_init_prefork_receive_setup_msg(lampiState_t *);
void lampi_init_prefork_receive_setup_params(lampiState_t *);
void lampi_init_prefork_resource_management(lampiState_t *);
void lampi_init_prefork_resources(lampiState_t *);
void lampi_init_prefork_stdio(lampiState_t *);

void lampi_init_postfork_debugger(lampiState_t *);
void lampi_init_postfork_globals(lampiState_t *);
void lampi_init_postfork_paths(lampiState_t *);
void lampi_init_postfork_resource_management(lampiState_t *);
void lampi_init_postfork_resources(lampiState_t *);
void lampi_init_postfork_stdio(lampiState_t *);

void lampi_init_allforked_resource_management(lampiState_t *);
void lampi_init_allforked_resources(lampiState_t *);

void lampi_daemon_loop(lampiState_t *);
void lampi_init_check_for_error(lampiState_t *);
void lampi_init_debug(lampiState_t *);
void lampi_init_fork(lampiState_t *);

void lampi_init_wait_for_start_message(lampiState_t *);


/*
 * prototypes for path specific initialization phases
 */
typedef void (*lampi_init_func_t) (lampiState_t *);

#ifdef USE_RMS
void lampi_init_prefork_rms(lampiState_t *);
void lampi_init_postfork_rms(lampiState_t *);
void lampi_init_allforked_rms(lampiState_t *);
#else
extern lampi_init_func_t lampi_init_prefork_rms;
extern lampi_init_func_t lampi_init_postfork_rms;
extern lampi_init_func_t lampi_init_allforked_rms;
#endif

#ifdef GM
void lampi_init_prefork_gm(lampiState_t *);
void lampi_init_prefork_receive_setup_msg_gm(lampiState_t *);
void lampi_init_prefork_receive_setup_params_gm(lampiState_t *);
void lampi_init_postfork_gm(lampiState_t *);
#else
extern lampi_init_func_t lampi_init_prefork_gm;
extern lampi_init_func_t lampi_init_prefork_receive_setup_msg_gm;
extern lampi_init_func_t lampi_init_prefork_receive_setup_params_gm;
extern lampi_init_func_t lampi_init_postfork_gm;
#endif

#ifdef QUADRICS
void lampi_init_prefork_quadrics(lampiState_t *);
void lampi_init_prefork_receive_setup_msg_quadrics(lampiState_t *);
void lampi_init_prefork_receive_setup_params_quadrics(lampiState_t *);
void lampi_init_postfork_quadrics(lampiState_t *);
#else
extern lampi_init_func_t lampi_init_prefork_quadrics;
extern lampi_init_func_t lampi_init_prefork_receive_setup_msg_quadrics;
extern lampi_init_func_t lampi_init_prefork_receive_setup_params_quadrics;
extern lampi_init_func_t lampi_init_postfork_quadrics;
#endif

#ifdef SHARED_MEMORY
void lampi_init_prefork_local_coll(lampiState_t *);
void lampi_init_postfork_local_coll(lampiState_t *);
void lampi_init_prefork_shared_memory(lampiState_t *);
void lampi_init_prefork_receive_setup_msg_shared_memory(lampiState_t *);
void lampi_init_prefork_receive_setup_params_shared_memory(lampiState_t *);
void lampi_init_postfork_shared_memory(lampiState_t *);
#else
extern lampi_init_func_t lampi_init_prefork_local_coll;
extern lampi_init_func_t lampi_init_postfork_local_coll;
extern lampi_init_func_t lampi_init_prefork_shared_memory;
extern lampi_init_func_t lampi_init_prefork_receive_setup_msg_shared_memory;
extern lampi_init_func_t lampi_init_prefork_receive_setup_params_shared_memory;
extern lampi_init_func_t lampi_init_postfork_shared_memory;
#endif

#ifdef UDP
void lampi_init_prefork_udp(lampiState_t *);
void lampi_init_postfork_udp(lampiState_t *);
#else
extern lampi_init_func_t lampi_init_prefork_udp;
extern lampi_init_func_t lampi_init_postfork_udp;
#endif
#endif /* _LAMPI_INIT_H_ */
