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

/*
 *  CommandArg.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 16 2003.
 */

#include "CommandArg.h"

static CommandArg	*_carg = NULL;

CommandArg *CommandArg::sharedCommandArg()
{
	return _carg;
}

void CommandArg::setSharedCommandArg(CommandArg *arg)
{
	delete _carg;
	_carg = arg;
}
	
CommandArg::CommandArg(int argc, const char **argv)
{
	/* parse command line arguments.  check for - and -- */
	/* ASSERT: each command-line switch must look like:
		-x [option]
		--longx [option]
	*/
	int			i;
	const char	*key;
	const char	*val;
	
	argc_m = argc;
	argv_m = argv;
	for ( i = 1; i < argc; i++ )
	{
		if ( ('-' == argv[i][0]) && ('-' == argv[i][1]) )
			key = &argv[i][2];
		else
			key = &argv[i][1];
		
		val = NULL;
		/* look ahead to see if a value has been defined. */
		if ( NULL != argv[i+1] )
			if ( ('-' != argv[i+1][0]) )
			{
				val = argv[i+1];
			}
			
		if ( NULL != val )
		{
			tbl_m.setValueForKey(val, key);
			i++;
		}
		else
			tbl_m.setValueForKey("", key);
	}
}
	
	
	
bool CommandArg::argumentSet(const char *option)
{
	return !(NULL == tbl_m.valueForKey(option) );
}

const char *CommandArg::valueForArgument(const char *option)
{
	return (const char *)tbl_m.valueForKey(option);
}
