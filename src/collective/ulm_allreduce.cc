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

#include <stdio.h>
#include <string.h>

#include "queue/globals.h"
#include "ulm/ulm.h"
#include "internal/log.h"
#include "internal/type_copy.h"

/*!
 * ulm_allreduce - reduce function entry point
 *
 * \param s_buf	Initial data
 * \param r_buf	        Buffer to receive the reduced data
 * \param count		Number of objects
 * \param type		Data type of objects
 * \param op		Operation operation
 * \param comm		The communicator ID
 * \return		ULM error code
 *
 * Description
 *
 * This function performs a reduce operation (such as sum, max,
 * logical and, etc.) across all the members of the group specified by
 * the context ID, and returns the result of the reduction at all
 * processes.
 *
 * The reduction operation can be either one of a predefined list of
 * operations, or a user-defined operation.
 *
 * This top-level entry point selects the appropriate algorithm.
 *
 * The algorithm is called blockwise to moderate the load for very
 * large data sets.
 */
extern "C" int ulm_allreduce(const void *s_buf,
                             void *r_buf,
                             int count,
                             ULMType_t *type,
                             ULMOp_t *op,
                             int comm)
{
    enum {
        USE_P2P = 0,
        KILOBYTE = 1 << 10,
        MEGABYTE = 1 << 20,
        BLOCK_SIZE = 1 * MEGABYTE
    };

    Communicator *communicator;
    Group *group;
    int block_count;
    int n;
    int rc;
    ulm_allreduce_t *algorithm;
    unsigned char *rp;
    unsigned char *sp;

    extern ulm_allreduce_t ulm_allreduce_linear;
    extern ulm_allreduce_t ulm_allreduce_p2p;

    communicator = communicators[comm];
    group = communicator->localGroup;

    /*
     * Select algorithm based on arguments
     */

    if (group->groupSize == 1) {

        if (s_buf != MPI_IN_PLACE) {
            type_copy(r_buf, s_buf, count, type);
        }

        return ULM_SUCCESS;

    } else if (op->commute == 0) {

        /*
         * For non-commuting operators, where evaluation order
         * matters, use a linear algorithm.  Otherwise use the fastest
         * available.
         */

        algorithm = ulm_allreduce_linear;

    }  else if (USE_P2P ||
                communicator->useSharedMemForCollectives == 0 ||
                type->extent > communicator->collectiveOpt.maxReduceExtent) {

        /*
         * Point-to-point algorithm
         */

        algorithm = ulm_allreduce_p2p;

    } else {

        /*
         * Intra-host / inter-host algorithm
         * 
         * For allreduce: do a reduce and then a bcast both of which
         * are optimized for intra-/inter-host using shared memory.
         */

        rc = ulm_reduce(s_buf, r_buf, count, type, op, 0, comm);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
        rc = ulm_bcast(r_buf, count, type, 0, comm);

        return rc;
    }

    /*
     * For explicit allreduce algorithms, call the algorithm blockwise
     */

    block_count = BLOCK_SIZE / type->packed_size;
    if (block_count == 0) {
        block_count = 1;
    }
    rp = (unsigned char *) r_buf;
    sp = (unsigned char *) s_buf;
    while (count > 0) {
        n = block_count < count ? block_count : count;
        rc = algorithm(sp, rp, n, type, op, comm);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
        count -= n;
        rp += n * type->extent;
        if (sp != (unsigned char *) MPI_IN_PLACE) {
            sp += n * type->extent;
        }
    }

    return ULM_SUCCESS;
}
