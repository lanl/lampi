/*
 *  Private.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Mon Jan 13 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _MPI_PRIVATE_H_
#define _MPI_PRIVATE_H_

#include <internal/MPIIncludes.h>
#include <internal/log.h>

/*
 *		MACRO Definitions
 */
 
#define ct_err(x)	do {\
	_ulm_err ("[%s:%d] ", _ulm_host(), getpid()); \
    _ulm_set_file_line(__FILE__, __LINE__) ; _ulm_err x ;  \
	fflush(stderr); } while (0)

#ifdef CT_DEBUG
#define ct_dbg(x)		ct_err(x)
#else
#define ct_dbg(x)
#endif



#endif
