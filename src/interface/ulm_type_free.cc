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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal/malloc.h"
#include "internal/mpi.h"
#include "internal/mpif.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

/*!
 * Free the resources associated with a type.  This does not do
 * reference count manipulation, so don't use this unless you
 * understand what you are doing; use the macros ulm_type_retain and
 * ulm_type_release instead instead.
 *
 * \param type		Communicator ID
 * \param errorCode	Exit code to pass to exit()
 * \return		Function never returns
 */
extern "C" int ulm_type_free(ULMType_t *type)
{
    if (type) {
        // remove fortran handle:
        if (_mpi.fortran_layer_enabled && type->fhandle != -1) {
            _mpi_ptr_table_free(_mpif.type_table, type->fhandle);
            type->fhandle = -1;
        }
 
        // free the envelope's integer array
        if ((type->envelope.nints > 0) && (type->envelope.iarray != NULL)) {
            ulm_free(type->envelope.iarray);
        }
        // free the envelope's address array
        if ((type->envelope.naddrs > 0) && (type->envelope.aarray != NULL)) {
            ulm_free(type->envelope.aarray);
        }

        // free the envelope's datatype array after potentially
        // freeing the referenced datatypes...
        if ((type->envelope.ndatatypes > 0) && (type->envelope.darray != NULL)) {
            for (int i = 0; i < type->envelope.ndatatypes; i++) {
                ULMType_t *t = (ULMType_t *) type->envelope.darray[i];
                // a little bit of recursion never hurt anyone...I hope...
                ulm_type_release(t);
            }
            ulm_free(type->envelope.darray);
        }
        if (type->type_map) {
            ulm_free(type->type_map);
        }
        ulm_free(type);
    }

    return ULM_SUCCESS;
}
