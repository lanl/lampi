/*
 *  misc.c
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 24 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "misc.h"

void free_double_carray(char **arr, int len)
{
	int		i;
	
	if ( !arr )
		return;
		
	for (i = 0; i<len; i++)
		free(arr[i]);
	free(arr);
}


void free_double_iarray(int **arr, int len)
{
	int		i;
	
	if ( !arr )
		return;
		
	for (i = 0; i<len; i++)
		free(arr[i]);
	free(arr);
}


unsigned int ulm_log2(unsigned int n)
{
	int		overflow;
	unsigned int	cnt, nn;
	
	cnt = 0;
	nn = n;
	while ( nn >>= 1 )
	{
		cnt++;
	}
	overflow = (~(1 << cnt) & n) > 0;
	
	return cnt + overflow;
	
}

unsigned int bit_string_to_uint(const char *bstr)
{
	unsigned int	num,  i;
	const char		*ptr;
	
	num = i = 0;
	ptr = bstr + strlen(bstr) - 1;
	while ( ptr >= bstr )
	{
		num += ((*ptr - '0') << i);
		ptr--;
		i++;
	}
	return num;
}


char *uint_to_bit_string(unsigned int num, int pad)
{
	char	*bstr;
	unsigned int	idx, svd, digits;
	int		i;
	
	bstr = malloc(sizeof(char)*8*sizeof(unsigned int) + 1);
	idx = 1 << (8*sizeof(unsigned int) - 1);
	//   skip leading 0s
	while ( !( idx & num ) &&  idx )
		idx = idx >> 1;
	
	if ( !idx )
	{
		if ( !pad )
			strcpy(bstr, "0");
		else
		{
			for ( i = 0; i < pad; i++ )
				bstr[i] = '0';
			bstr[pad] = '\0';
		}
		return bstr;
	}
	
	svd = idx;
	digits = 1;
	while ( (svd = svd >> 1) )
		digits++;
	
	for ( i = 0; i < (int)pad - (int)digits; i++ )
		bstr[i] = '0';
		
	while ( idx )
	{
		bstr[i++] = ( idx & num ) ? '1' : '0';
		idx = idx >> 1;
	}
	bstr[i] = '\0';
	
	return bstr;
}

