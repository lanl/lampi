/*
 *  CommandArg.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 16 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
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
