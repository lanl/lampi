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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * File:	ulm_binary_functions.c
 *
 * Description:
 *
 * Binary associative functions used as arguments to the the reduction
 * collective operations: ulm_allreduce, ulm_scan, ...
 *
 */

/* Header files *******************************************************/

#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"
/* Macros *************************************************************/

#define _ULM_BAND(x, y)	((x)&(y))
#define _ULM_BOR(x, y)	((x)|(y))
#define _ULM_BXOR(x, y)	((x)^(y))
#define _ULM_LAND(x, y)	((x)&&(y))
#define _ULM_LOR(x, y)	((x)||(y))
#define _ULM_LXOR(x, y)	(((x)&&(!y))||((!x)&&(y)))
#define _ULM_MAX(x, y)	(((y)>(x))?(y):(x))
#define _ULM_MIN(x, y)	(((x)>(y))?(y):(x))
#define _ULM_PROD(x, y)	((x)*(y))
#define _ULM_SUM(x, y)	((x)+(y))

/* restrict macro for the basic
 *  datatypes
 */
#define RCHAR_PTR     char*  RESTRICT_MACRO
#define RINT_PTR      int*   RESTRICT_MACRO
#define RLONG_PTR     long*  RESTRICT_MACRO
#define RSHORT_PTR    short* RESTRICT_MACRO
#define RFLOAT_PTR    float* RESTRICT_MACRO
#define RDOUBLE_PTR   double* RESTRICT_MACRO
#define RLONGDOUBLE_PTR  long double* RESTRICT_MACRO
#define RLONGLONG_PTR  long long* RESTRICT_MACRO
/*   restrict macro for the unsigned
 *     datatypes
 */
#define RUCHAR_PTR   unsigned char* RESTRICT_MACRO
#define RUINT_PTR    unsigned int*  RESTRICT_MACRO
#define RULONG_PTR   unsigned long* RESTRICT_MACRO
#define RUSHORT_PTR  unsigned short* RESTRICT_MACRO


/* Functions **********************************************************/

extern "C" void ulm_null(void *inv, void *inoutv, int *pn, void *dummy)
{
    return;
}


extern "C" void ulm_cmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_cmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_csum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_cprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_cbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_cband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_cbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_clor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_cland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_clxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RCHAR_PTR a = (char *) inoutv;
    RCHAR_PTR b = (char *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_smax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_smin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ssum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_sprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_sbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_sband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_sbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_slor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_sland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_slxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RSHORT_PTR a = (short *) inoutv;
    RSHORT_PTR b = (short *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_imax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b  = (int *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_imin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_isum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR  a = (int *) inoutv;
    RINT_PTR  b = (int *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_iprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR  b = (int *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_ibor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_iband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_ibxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_ilor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_iland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ilxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RINT_PTR a = (int *) inoutv;
    RINT_PTR b = (int *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_lmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_lmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_lsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_lprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_lbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_lband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_lbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_llor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_lland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_llxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONG_PTR a = (long *) inoutv;
    RLONG_PTR b = (long *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ucmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ucmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ucsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_ucprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_ucbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_ucband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_ucbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_uclor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ucland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uclxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUCHAR_PTR a = (unsigned char *) inoutv;
    RUCHAR_PTR b = (unsigned char *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_usmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_usmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ussum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_usprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_usbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_usband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_usbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_uslor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_usland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uslxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUSHORT_PTR a = (unsigned short *) inoutv;
    RUSHORT_PTR b = (unsigned short *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uimax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uimin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uisum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_uiprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_uibor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_uiband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_uibxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_uilor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uiland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_uilxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RUINT_PTR a = (unsigned int *) inoutv;
    RUINT_PTR b = (unsigned int *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ulmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ulmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ulsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_ulprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_ulbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_ulband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_ulbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_ullor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ulland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_ullxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RULONG_PTR a = (unsigned long *) inoutv;
    RULONG_PTR b = (unsigned long *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_fmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RFLOAT_PTR a = (float *) inoutv;
    RFLOAT_PTR b = (float *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_fmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RFLOAT_PTR a = (float *) inoutv;
    RFLOAT_PTR b = (float *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_fsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RFLOAT_PTR a = (float *) inoutv;
    RFLOAT_PTR b = (float *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_fprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RFLOAT_PTR a = (float *) inoutv;
    RFLOAT_PTR b = (float *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_dmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RDOUBLE_PTR a = (double *) inoutv;
    RDOUBLE_PTR b = (double *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_dmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RDOUBLE_PTR a = (double *) inoutv;
    RDOUBLE_PTR b = (double *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_dsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RDOUBLE_PTR a = (double *) inoutv;
    RDOUBLE_PTR b = (double *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_dprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RDOUBLE_PTR a = (double *) inoutv;
    RDOUBLE_PTR b = (double *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_qmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGDOUBLE_PTR a = (long double *) inoutv;
    RLONGDOUBLE_PTR b = (long double *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_qmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGDOUBLE_PTR a = (long double *) inoutv;
    RLONGDOUBLE_PTR b = (long double *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_qsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGDOUBLE_PTR a = (long double *) inoutv;
    RLONGDOUBLE_PTR b = (long double *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_qprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGDOUBLE_PTR a = (long double *) inoutv;
    RLONGDOUBLE_PTR b = (long double *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


/*
 * Complex functions (for fortran)
 */


extern "C" void ulm_scsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_scomplex_t *a = (ulm_scomplex_t *) inoutv;
    ulm_scomplex_t *b = (ulm_scomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re + b[i].re;
	a[i].im = a[i].im + b[i].im;
    }
}


extern "C" void ulm_scprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_scomplex_t *a = (ulm_scomplex_t *) inoutv;
    ulm_scomplex_t *b = (ulm_scomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re * b[i].re - a[i].im * b[i].im;
	a[i].im = a[i].re * b[i].im + a[i].im * b[i].re;
    }
}


extern "C" void ulm_dcsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_dcomplex_t *a = (ulm_dcomplex_t *) inoutv;
    ulm_dcomplex_t *b = (ulm_dcomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re + b[i].re;
	a[i].im = a[i].im + b[i].im;
    }
}


extern "C" void ulm_dcprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_dcomplex_t *a = (ulm_dcomplex_t *) inoutv;
    ulm_dcomplex_t *b = (ulm_dcomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re * b[i].re - a[i].im * b[i].im;
	a[i].im = a[i].re * b[i].im + a[i].im * b[i].re;
    }
}


extern "C" void ulm_qcsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_qcomplex_t *a = (ulm_qcomplex_t *) inoutv;
    ulm_qcomplex_t *b = (ulm_qcomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re + b[i].re;
	a[i].im = a[i].im + b[i].im;
    }
}


extern "C" void ulm_qcprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_qcomplex_t *a = (ulm_qcomplex_t *) inoutv;
    ulm_qcomplex_t *b = (ulm_qcomplex_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	a[i].re = a[i].re * b[i].re - a[i].im * b[i].im;
	a[i].im = a[i].re * b[i].im + a[i].im * b[i].re;
    }
}


/*
 * maxloc and minloc functions
 */

extern "C" void ulm_cmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_char_int_t *a = (ulm_char_int_t *) inoutv;
    ulm_char_int_t *b = (ulm_char_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_smaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_short_int_t *a = (ulm_short_int_t *) inoutv;
    ulm_short_int_t *b = (ulm_short_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_imaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_int_int_t *a = (ulm_int_int_t *) inoutv;
    ulm_int_int_t *b = (ulm_int_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_lmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_long_int_t *a = (ulm_long_int_t *) inoutv;
    ulm_long_int_t *b = (ulm_long_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ucmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_uchar_int_t *a = (ulm_uchar_int_t *) inoutv;
    ulm_uchar_int_t *b = (ulm_uchar_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_usmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ushort_int_t *a = (ulm_ushort_int_t *) inoutv;
    ulm_ushort_int_t *b = (ulm_ushort_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_uimaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_uint_int_t *a = (ulm_uint_int_t *) inoutv;
    ulm_uint_int_t *b = (ulm_uint_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ulmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ulong_int_t *a = (ulm_ulong_int_t *) inoutv;
    ulm_ulong_int_t *b = (ulm_ulong_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_fmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_float_int_t *a = (ulm_float_int_t *) inoutv;
    ulm_float_int_t *b = (ulm_float_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_dmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_double_int_t *a = (ulm_double_int_t *) inoutv;
    ulm_double_int_t *b = (ulm_double_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_qmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ldouble_int_t *a = (ulm_ldouble_int_t *) inoutv;
    ulm_ldouble_int_t *b = (ulm_ldouble_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ffmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_float_float_t *a = (ulm_float_float_t *) inoutv;
    ulm_float_float_t *b = (ulm_float_float_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ddmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_double_double_t *a = (ulm_double_double_t *) inoutv;
    ulm_double_double_t *b = (ulm_double_double_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_qqmaxloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ldouble_ldouble_t *a = (ulm_ldouble_ldouble_t *) inoutv;
    ulm_ldouble_ldouble_t *b = (ulm_ldouble_ldouble_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MIN(b[i].loc, a[i].loc);
	} else if (b[i].val > a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_cminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_char_int_t *a = (ulm_char_int_t *) inoutv;
    ulm_char_int_t *b = (ulm_char_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_sminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_short_int_t *a = (ulm_short_int_t *) inoutv;
    ulm_short_int_t *b = (ulm_short_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_iminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_int_int_t *a = (ulm_int_int_t *) inoutv;
    ulm_int_int_t *b = (ulm_int_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_lminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_long_int_t *a = (ulm_long_int_t *) inoutv;
    ulm_long_int_t *b = (ulm_long_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ucminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_uchar_int_t *a = (ulm_uchar_int_t *) inoutv;
    ulm_uchar_int_t *b = (ulm_uchar_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_usminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ushort_int_t *a = (ulm_ushort_int_t *) inoutv;
    ulm_ushort_int_t *b = (ulm_ushort_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_uiminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_uint_int_t *a = (ulm_uint_int_t *) inoutv;
    ulm_uint_int_t *b = (ulm_uint_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ulminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ulong_int_t *a = (ulm_ulong_int_t *) inoutv;
    ulm_ulong_int_t *b = (ulm_ulong_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_fminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_float_int_t *a = (ulm_float_int_t *) inoutv;
    ulm_float_int_t *b = (ulm_float_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_dminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_double_int_t *a = (ulm_double_int_t *) inoutv;
    ulm_double_int_t *b = (ulm_double_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_qminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ldouble_int_t *a = (ulm_ldouble_int_t *) inoutv;
    ulm_ldouble_int_t *b = (ulm_ldouble_int_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ffminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_float_float_t *a = (ulm_float_float_t *) inoutv;
    ulm_float_float_t *b = (ulm_float_float_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_ddminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_double_double_t *a = (ulm_double_double_t *) inoutv;
    ulm_double_double_t *b = (ulm_double_double_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

extern "C" void ulm_qqminloc(void *inv, void *inoutv, int *pn, void *dummy)
{
    ulm_ldouble_ldouble_t *a = (ulm_ldouble_ldouble_t *) inoutv;
    ulm_ldouble_ldouble_t *b = (ulm_ldouble_ldouble_t *) inv;
    int i;

    for (i = 0; i < *pn; i++) {
	if (b[i].val == a[i].val) {
	    a[i].loc = _ULM_MAX(b[i].loc, a[i].loc);
	} else if (b[i].val < a[i].val) {
	    a[i].val = b[i].val;
	    a[i].loc = b[i].loc;
	}
    }
}

/* long long functions */
extern "C" void ulm_llmax(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a = _ULM_MAX(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_llmin(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a = _ULM_MIN(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_llsum(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a++ += *b++;
    }
}


extern "C" void ulm_llprod(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a++ *= *b++;
    }
}


extern "C" void ulm_llbor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a++ |= *b++;
    }
}


extern "C" void ulm_llband(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a++ &= *b++;
    }
}


extern "C" void ulm_llbxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR  b = (long long *) inv;

    while (n--) {
	*a++ ^= *b++;
    }
}


extern "C" void ulm_lllor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a = _ULM_LOR(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_llland(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR a = (long long *) inoutv;
    RLONGLONG_PTR  b = (long long *) inv;

    while (n--) {
	*a = _ULM_LAND(*a, *b);
	a++;
	b++;
    }
}


extern "C" void ulm_lllxor(void *inv, void *inoutv, int *pn, void *dummy)
{
    int n = *pn;
    RLONGLONG_PTR  a=  (long long*) inoutv;
    RLONGLONG_PTR b = (long long *) inv;

    while (n--) {
	*a = _ULM_LXOR(*a, *b);
	a++;
	b++;
    }
}
