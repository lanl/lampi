/*
 *  misc.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 24 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _ULM_MISC_H_
#define _ULM_MISC_H_

#include <internal/MPIIncludes.h>

#ifdef __cplusplus
extern "C"
{
#endif

void free_double_carray(char **arr, int len);
void free_double_iarray(int **arr, int len);

unsigned int ulm_log2(unsigned int n);

unsigned int bit_string_to_uint(const char *bstr);
char *uint_to_bit_string(unsigned int num, int pad);

//unsigned int hostToIPAddress(const char *host);

#ifdef __cplusplus
}
#endif

#endif
