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

#include <string.h>
#include <stdlib.h>

#include "queue/globals.h"
#include "internal/log.h"
#include "ulm/ulm.h"

/*!
 * Get information about a specific communicator
 *
 * \param comm		ID for the communicator of interest.
 * \param key		Keyword for the data of interest.
 * \param buffer	Buffer to hold the returned info.
 * \param size		Size of buffer in bytes.
 *
 * Return the information specified by key for the given comm.
 * If comm < 0, then return global information.
 *
 */
extern "C" int ulm_get_info(int comm, ULMInfo_t key,
			    void *buffer, size_t size)
{
    int rc = ULM_ERROR;
    int nhosts, host_id, num_procs;

    if (ENABLE_CHECK_API_ARGS) {
        if (comm >= 0 && communicators[comm] == 0L) {
            ulm_err(("Error: ulm_get_info: bad communicator %d\n", comm));
            return ULM_ERR_COMM;
        }
        if (key < 0 || key >= _ULM_INFO_END_) {
            ulm_err(("Error: ulm_get_info: bad key %d\n", key));
            return ULM_ERR_BAD_PARAM;
        }
        if (buffer == NULL) {
            ulm_err(("Error: ulm_get_info: bad buffer %p\n", buffer));
            return ULM_ERR_BUFFER;
        }
        if (size == 0) {
            ulm_err(("Error: ulm_get_info: bad size %ld\n", size));
            return ULM_ERR_BAD_PARAM;
        }
    }

    if (comm < 0) {

        /*
         * For a null context, return global information
         */

        switch (key) {

        case ULM_INFO_PROCID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = myproc();
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_NUMBER_OF_PROCS:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = nprocs();
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_HOSTID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = myhost();
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_NUMBER_OF_HOSTS:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = lampiState.nhosts;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_ON_HOST_PROCID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = local_myproc();
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_ON_HOST_NUMBER_OF_PROCS:
            nhosts = communicators[comm]->localGroup->numberOfHostsInGroup;
            if (size >= nhosts * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < nhosts; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[i].nGroupProcIDOnHost;
                }
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_NUMBER_OF_PROCS:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->groupHostData[host_id].nGroupProcIDOnHost;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_PROCIDS:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            num_procs = communicators[comm]->localGroup->groupHostData[host_id].nGroupProcIDOnHost;
            if (size >= num_procs * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < num_procs; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[host_id].
                        groupProcIDOnHost[i];
                }
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_COMM_ROOT:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            if (size >= sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->groupHostData[host_id].hostCommRoot;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_HOST_COMM_ROOTS:
            nhosts = communicators[comm]->localGroup->numberOfHostsInGroup;
            if (size >= nhosts * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < nhosts; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[i].hostCommRoot;
                }
                rc = ULM_SUCCESS;
            }
            break;

        default:
            break;
        }

    } else {

        /*
         * Get information relative to the context
         */

        switch (key) {

        case ULM_INFO_PROCID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->ProcID;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_NUMBER_OF_PROCS:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->groupSize;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_HOSTID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->hostIndexInGroup;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_NUMBER_OF_HOSTS:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->numberOfHostsInGroup;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_ON_HOST_PROCID:
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->onHostProcID;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_ON_HOST_NUMBER_OF_PROCS:
            nhosts = communicators[comm]->localGroup->numberOfHostsInGroup;
            if (size >= nhosts * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < nhosts; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[i].nGroupProcIDOnHost;
                }
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_NUMBER_OF_PROCS:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            if (size == sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->groupHostData[host_id].nGroupProcIDOnHost;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_PROCIDS:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            num_procs = communicators[comm]->localGroup->groupHostData[host_id].nGroupProcIDOnHost;
            if (size >= num_procs * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < num_procs; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[host_id].
                        groupProcIDOnHost[i];
                }
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_THIS_HOST_COMM_ROOT:
            host_id = communicators[comm]->localGroup->hostIndexInGroup;
            if (size >= sizeof(int)) {
                int *val = (int *) buffer;
                *val = communicators[comm]->localGroup->groupHostData[host_id].hostCommRoot;
                rc = ULM_SUCCESS;
            }
            break;

        case ULM_INFO_HOST_COMM_ROOTS:
            nhosts = communicators[comm]->localGroup->numberOfHostsInGroup;
            if (size >= nhosts * sizeof(int)) {
                int i;
                int *val = (int *) buffer;
                memset(buffer, 0, size);
                for (i = 0; i < nhosts; i++) {
                    val[i] = communicators[comm]->localGroup->groupHostData[i].hostCommRoot;
                }
                rc = ULM_SUCCESS;
            }
            break;

        default:
            break;
        }
    }

    return rc;
}
