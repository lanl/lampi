/*
 *  parsing.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Sat Jan 18 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _ULM_PARSING_H_
#define _ULM_PARSING_H_

#include <internal/MPIIncludes.h>

#ifdef __cplusplus
extern "C"
{
#endif


int _ulm_parse_string(char ***ArgList, const char* InputBuffer, const int NSeparators,
		     const char *SeparatorList);
/*
	This function will parse the input string in InputBuffer and
	return an array of substrings that are separated by the characters
	identified by SeparatorList.
	PRE:	SeparatorList is an array of length NSeparators where each
			character is a delimiter for InputBuffer.
			ArgList is a reference parameter for an array of strings.
	POST:	function will create an array of substrings that are
			delimited by the chars in SeparatorList.  Caller is responsible
			for freeing the memory.
*/


#ifdef __cplusplus
}
#endif

#endif
