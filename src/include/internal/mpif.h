/*
 * Copyright 2002-2003. The Regents of the University of
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

#ifndef _ULM_INTERNAL_MPIF_H_
#define _ULM_INTERNAL_MPIF_H_

#include "internal/mpi.h"
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && (__GNUC__ >= 3)
#include "internal/mpif_decls.h"
#endif

CDECL_BEGIN

/*
 * macros
 */

#define INLINE		__inline
#define LOOKUP_OP(X)	((MPI_Op) _mpif.op_table->addr[(X)])
#define LOOKUP_REQ(X)	((MPI_Request *) &(_mpif.request_table->addr[(X)]))
#define LOOKUP_TYPE(X)	((MPI_Datatype) _mpif.type_table->addr[(X)])

/*
 * state
 */

typedef struct mpif_state_t mpif_state_t;
struct mpif_state_t {
    lockStructure_t lock;
    int initialized;
    int finalized;
    ptr_table_t	*op_table;
    ptr_table_t	*request_table;
    ptr_table_t	*type_table;
};

/*
 * external variables
 */

extern mpif_state_t _mpif;

/*
 * prototypes of internal functions
 */

int _mpif_init(void);
int _mpi_finalize(void);
ptr_table_t *_mpif_create_op_table(void);
ptr_table_t *_mpif_create_request_table(void);
ptr_table_t *_mpif_create_type_table(void);

CDECL_END

#endif /* !_ULM_INTERNAL_MPIF_H_ */
