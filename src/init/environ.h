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



#ifndef _LAMPI_ENVIRON_H_
#define _LAMPI_ENVIRON_H_

#include "internal/linkage.h"

#define E0	26
#define E1	10

/*
	This is the central interface for getting environment variable
	values.  lampi_environ_init should be called prior to any other
	lampi_environ_* function calls.
*/


CDECL_BEGIN

enum
{
	LAMPI_ENV_ERR_NULL_NAME = 1,
	LAMPI_ENV_ERR_NOT_FOUND
};


void lampi_environ_init(void);
void lampi_environ_dump(void);

/*
	All of the lampi_environ_find_* functions return LAMPI_ENV_ERR_NULL_NAME
	if the name input is NULL, and LAMPI_ENV_ERR_NOT_FOUND if the environment
	variable identified by name does not exist.  If the variable exists,
	then the functions return 0.  All pass-by-reference params are initialized
	to 0 (NULL).
*/

int lampi_environ_find_integer(const char *name, int *eval);
/*
	POST:	If the environ variable exists, then the integer value
			is returned in eval.
*/

int lampi_environ_find_real(const char *name, double *eval);
/*
	POST:	If the environ variable exists, then the double value
			is returned in eval.
*/

int lampi_environ_find_string(const char *name, char **eval);
/*
	PRE:	eval should be the address of a pointer variable.
	POST:	If the environ variable exists, then the string value
			is returned in eval.  User should not free the string.
*/

int lampi_environ_find_string_array(const char *name, char ***eval);
/*
	PRE:	eval should be the address of a pointer variable that points
			to an array of strings.
	POST:	If the environ variable exists, then the address of the string
			array is returned in eval.  User should not free the array.
*/

CDECL_END


#endif /* _LAMPI_ENVIRON_H_ */
