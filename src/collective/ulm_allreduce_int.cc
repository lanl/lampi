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
 * Simplified allreduce function which acts on integer data types
 */

#include "ulm/ulm.h"

extern "C"
int ulm_allreduce_int(const void *sendbuf,
		      void *recvbuf,
		      int count,
		      ULMFunc_t *func,
		      int comm)
{
    int rc;
    ULMOp_t op;
    ULMFunc_t *funcarray[1];
    ULMType_t type;
    ULMTypeMapElt_t typemap;

    extern ulm_allreduce_t ulm_allreduce_p2p;

    // setup operation for allreduce
    op.func = funcarray;
    op.func[0] = func;
    op.isbasic = 0;
    op.commute = 1;
    op.fortran = 0;

    // setup integer type for allreduce
    typemap.size = sizeof(int);
    typemap.offset = 0;
    typemap.seq_offset = 0;
    type.type_map = &typemap;
    type.lower_bound = 0;
    type.extent = sizeof(signed int);
    type.packed_size = sizeof(signed int);
    type.num_pairs = 1;
    type.layout = CONTIGUOUS;
    type.isbasic = 0;
    type.op_index = 0;
    type.fhandle = -1;
    type.ref_count = 1;
    type.committed = 0;

    rc = ulm_allreduce_p2p(sendbuf, recvbuf, count, &type, &op, comm);

    return rc;
}
