/*
 *  CommandArg.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Thu Jan 16 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _COMMAND_ARG_H_
#define _COMMAND_ARG_H_

#include "internal/MPIIncludes.h"
#include "util/HashTable.h"

class CommandArg
{
	private:
	HashTable	tbl_m;
	int			argc_m;
	const char	**argv_m;
	
	public:
	static CommandArg *sharedCommandArg();
	static void setSharedCommandArg(CommandArg *arg);
	
	CommandArg(int argc, const char **argv);
	
	bool argumentSet(const char *option);
	const char *valueForArgument(const char *option);
	
};

#endif
