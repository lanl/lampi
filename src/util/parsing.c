/*
 *  parsing.c
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Sat Jan 18 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "parsing.h"
#include "linked_list.h"
#include "internal/malloc.h"

inline void _ulm_parse_add(char **substr, int cnt, const char *cptr, int len)
{
	substr[cnt] = (char *)ulm_malloc(sizeof(char)*(len+1));
	if ( NULL == substr[cnt] )
	{
		printf("Error: can't allocate memory in parser. Len %ld\n",
		(long)len);
		perror(" Memory allocation failure ");
		exit(-1);
	}
	/* copy substring. */
	strncpy(substr[cnt], cptr, len);
	substr[cnt][len] = '\0';
}




int _ulm_parse_string(char ***ArgList, const char* InputBuffer, const int NSeparators,
		     const char *SeparatorList)
{
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
	const char	*cptr, *ptr;
	int			substrCnt, cnt, totalCnt, len, blocks, i;
	char		**substr;
	
	/* Make an estimate of how many substrings there could be.
	This is a conservative estimate of the upper bound.
	Guess that the worst case is all substrings are of length 3.
	*/
	substrCnt = strlen(InputBuffer) / 3;
	substrCnt = (!substrCnt) ? 1 : substrCnt;	// alloc at least 1 string
	*ArgList = (char **)ulm_malloc(sizeof(char *)*substrCnt);
    if( NULL == *ArgList )
	{
		printf("Error: can't allocate memory in parser. Len %ld\n",
			(long)(sizeof(char *)*substrCnt));
		perror(" Memory allocation failure ");
		exit(-1);
    }
	bzero(*ArgList, substrCnt);
	blocks = 1;
		
	/* parse InputBuffer. */
	ptr = cptr = InputBuffer;
	totalCnt = cnt = 0;
	substr = *ArgList;
	while ( *ptr )
	{
		
		for (i = 0; i < NSeparators; i++)
			if ( *ptr == SeparatorList[i] )
				break;
				
		if ( i < NSeparators )
		{
			/* encountered delimiter */
			len = ptr - cptr;
			if ( len )
			{
				_ulm_parse_add(substr, cnt, cptr, len);
				totalCnt++;
				
				if ( ++cnt >= substrCnt )
				{
					blocks++;
					len = substrCnt*blocks;
					*ArgList = realloc(*ArgList, sizeof(char *)*len);
					substr = *ArgList + len - substrCnt;
					if( NULL == *ArgList )
					{
						printf("Error: can't allocate memory in parser. Len %ld\n",
							(long)(sizeof(char *)*substrCnt*blocks));
						perror(" Memory allocation failure ");
						exit(-1);
					}
					cnt = 0;
				}
			}
			cptr = ptr + 1;
		}
		ptr++;
	}
	/* check for last substring; InputBuffer may not end with
	a delimiter. */
	if ( (len = (ptr - cptr)) )
	{
		_ulm_parse_add(substr, cnt, cptr, len);
		totalCnt++;
	}
		
	return totalCnt;
}
