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

#include "internal/mpi.h"

/*
 * Pre-defined reduction operations
 */
MPI_Op MPI_MAX;
MPI_Op MPI_MIN;
MPI_Op MPI_SUM;
MPI_Op MPI_PROD;
MPI_Op MPI_MAXLOC;
MPI_Op MPI_MINLOC;
MPI_Op MPI_BAND;
MPI_Op MPI_BOR;
MPI_Op MPI_BXOR;
MPI_Op MPI_LAND;
MPI_Op MPI_LOR;
MPI_Op MPI_LXOR;

/*
 * MPI operation initialization
 *
 * Nothing for it but to be long-winded...
 */
int _mpi_init_operations(void)
{
    static ULMOp_t _MPI_MAX = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_MIN = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_SUM = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_PROD = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_MAXLOC = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_MINLOC = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_BAND = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_BOR = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_BXOR = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_LAND = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_LOR = {NULL, 1, 1, 0};
    static ULMOp_t _MPI_LXOR = {NULL, 1, 1, 0};

    MPI_MAX = &_MPI_MAX;
    MPI_MIN = &_MPI_MIN;
    MPI_SUM = &_MPI_SUM;
    MPI_PROD = &_MPI_PROD;
    MPI_MAXLOC = &_MPI_MAXLOC;
    MPI_MINLOC = &_MPI_MINLOC;
    MPI_BAND = &_MPI_BAND;
    MPI_BOR = &_MPI_BOR;
    MPI_BXOR = &_MPI_BXOR;
    MPI_LAND = &_MPI_LAND;
    MPI_LOR = &_MPI_LOR;
    MPI_LXOR = &_MPI_LXOR;

    _MPI_MAX.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_MAX.func == NULL) {
	return -1;
    }
    _MPI_MAX.func[_MPI_CHAR_OP_INDEX]			= ulm_cmax;
    _MPI_MAX.func[_MPI_SHORT_OP_INDEX]			= ulm_smax;
    _MPI_MAX.func[_MPI_INT_OP_INDEX]			= ulm_imax;
    _MPI_MAX.func[_MPI_LONG_OP_INDEX]			= ulm_lmax;
    _MPI_MAX.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucmax;
    _MPI_MAX.func[_MPI_UNSIGNED_SHORT_OP_INDEX]		= ulm_usmax;
    _MPI_MAX.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uimax;
    _MPI_MAX.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulmax;
    _MPI_MAX.func[_MPI_FLOAT_OP_INDEX]			= ulm_fmax;
    _MPI_MAX.func[_MPI_DOUBLE_OP_INDEX]			= ulm_dmax;
    _MPI_MAX.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_qmax;
    _MPI_MAX.func[_MPI_PACKED_OP_INDEX]			= ulm_null;
    _MPI_MAX.func[_MPI_BYTE_OP_INDEX]			= ulm_cmax;
    _MPI_MAX.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llmax;
    _MPI_MAX.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_MAX.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_MAX.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_MAX.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_MAX.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MAX.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_MIN.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_MIN.func == NULL) {
	return -1;
    }
    _MPI_MIN.func[_MPI_CHAR_OP_INDEX]			= ulm_cmin;
    _MPI_MIN.func[_MPI_SHORT_OP_INDEX]			= ulm_smin;
    _MPI_MIN.func[_MPI_INT_OP_INDEX]			= ulm_imin;
    _MPI_MIN.func[_MPI_LONG_OP_INDEX]			= ulm_lmin;
    _MPI_MIN.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucmin;
    _MPI_MIN.func[_MPI_UNSIGNED_SHORT_OP_INDEX]		= ulm_usmin;
    _MPI_MIN.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uimin;
    _MPI_MIN.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulmin;
    _MPI_MIN.func[_MPI_FLOAT_OP_INDEX]			= ulm_fmin;
    _MPI_MIN.func[_MPI_DOUBLE_OP_INDEX]			= ulm_dmin;
    _MPI_MIN.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_qmin;
    _MPI_MIN.func[_MPI_PACKED_OP_INDEX]			= ulm_null;
    _MPI_MIN.func[_MPI_BYTE_OP_INDEX]			= ulm_cmin;
    _MPI_MIN.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llmin;
    _MPI_MIN.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_MIN.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_MIN.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_MIN.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_MIN.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_MIN.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MIN.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_SUM.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_SUM.func == NULL) {
	return -1;
    }
    _MPI_SUM.func[_MPI_CHAR_OP_INDEX]			= ulm_csum;
    _MPI_SUM.func[_MPI_SHORT_OP_INDEX]			= ulm_ssum;
    _MPI_SUM.func[_MPI_INT_OP_INDEX]			= ulm_isum;
    _MPI_SUM.func[_MPI_LONG_OP_INDEX]			= ulm_lsum;
    _MPI_SUM.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucsum;
    _MPI_SUM.func[_MPI_UNSIGNED_SHORT_OP_INDEX]		= ulm_ussum;
    _MPI_SUM.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uisum;
    _MPI_SUM.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulsum;
    _MPI_SUM.func[_MPI_FLOAT_OP_INDEX]			= ulm_fsum;
    _MPI_SUM.func[_MPI_DOUBLE_OP_INDEX]			= ulm_dsum;
    _MPI_SUM.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_qsum;
    _MPI_SUM.func[_MPI_PACKED_OP_INDEX]			= ulm_null;
    _MPI_SUM.func[_MPI_BYTE_OP_INDEX]			= ulm_csum;
    _MPI_SUM.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llsum;
    _MPI_SUM.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_SUM.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_SUM.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_SUM.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_SUM.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_SUM.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_SUM.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_SUM.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_SUM.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_SUM.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_scsum;
    _MPI_SUM.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_dcsum;
    _MPI_SUM.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_qcsum;

    _MPI_PROD.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_PROD.func == NULL) {
	return -1;
    }
    _MPI_PROD.func[_MPI_CHAR_OP_INDEX]			= ulm_cprod;
    _MPI_PROD.func[_MPI_SHORT_OP_INDEX]			= ulm_sprod;
    _MPI_PROD.func[_MPI_INT_OP_INDEX]			= ulm_iprod;
    _MPI_PROD.func[_MPI_LONG_OP_INDEX]			= ulm_lprod;
    _MPI_PROD.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucprod;
    _MPI_PROD.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_usprod;
    _MPI_PROD.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uiprod;
    _MPI_PROD.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulprod;
    _MPI_PROD.func[_MPI_FLOAT_OP_INDEX]			= ulm_fprod;
    _MPI_PROD.func[_MPI_DOUBLE_OP_INDEX]		= ulm_dprod;
    _MPI_PROD.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_qprod;
    _MPI_PROD.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_BYTE_OP_INDEX]			= ulm_cprod;
    _MPI_PROD.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llprod;
    _MPI_PROD.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_PROD.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_PROD.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_PROD.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_PROD.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_PROD.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_scprod;
    _MPI_PROD.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_dcprod;
    _MPI_PROD.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_qcprod;

    _MPI_MAXLOC.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_MAXLOC.func == NULL) {
	return -1;
    }
    _MPI_MAXLOC.func[_MPI_CHAR_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_SHORT_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_INT_OP_INDEX]			= ulm_null;
    _MPI_MAXLOC.func[_MPI_LONG_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_UNSIGNED_CHAR_OP_INDEX]	= ulm_null;
    _MPI_MAXLOC.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_null;
    _MPI_MAXLOC.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_UNSIGNED_LONG_OP_INDEX]	= ulm_null;
    _MPI_MAXLOC.func[_MPI_FLOAT_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_BYTE_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_fmaxloc;
    _MPI_MAXLOC.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_dmaxloc;
    _MPI_MAXLOC.func[_MPI_LONG_INT_OP_INDEX]		= ulm_lmaxloc;
    _MPI_MAXLOC.func[_MPI_2INT_OP_INDEX]		= ulm_imaxloc;
    _MPI_MAXLOC.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_smaxloc;
    _MPI_MAXLOC.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_qmaxloc;
    _MPI_MAXLOC.func[_MPI_2INTEGER_OP_INDEX]		= ulm_imaxloc;
    _MPI_MAXLOC.func[_MPI_2REAL_OP_INDEX]		= ulm_ffmaxloc;
    _MPI_MAXLOC.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_ddmaxloc;
    _MPI_MAXLOC.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MAXLOC.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_MINLOC.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_MINLOC.func == NULL) {
	return -1;
    }
    _MPI_MINLOC.func[_MPI_CHAR_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_SHORT_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_INT_OP_INDEX]			= ulm_null;
    _MPI_MINLOC.func[_MPI_LONG_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_UNSIGNED_CHAR_OP_INDEX]	= ulm_null;
    _MPI_MINLOC.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_null;
    _MPI_MINLOC.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_UNSIGNED_LONG_OP_INDEX]	= ulm_null;
    _MPI_MINLOC.func[_MPI_FLOAT_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_BYTE_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_fminloc;
    _MPI_MINLOC.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_dminloc;
    _MPI_MINLOC.func[_MPI_LONG_INT_OP_INDEX]		= ulm_lminloc;
    _MPI_MINLOC.func[_MPI_2INT_OP_INDEX]		= ulm_iminloc;
    _MPI_MINLOC.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_sminloc;
    _MPI_MINLOC.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_qminloc;
    _MPI_MINLOC.func[_MPI_2INTEGER_OP_INDEX]		= ulm_iminloc;
    _MPI_MINLOC.func[_MPI_2REAL_OP_INDEX]		= ulm_ffminloc;
    _MPI_MINLOC.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_ddminloc;
    _MPI_MINLOC.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_MINLOC.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_BAND.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_BAND.func == NULL) {
	return -1;
    }
    _MPI_BAND.func[_MPI_CHAR_OP_INDEX]			= ulm_cband;
    _MPI_BAND.func[_MPI_SHORT_OP_INDEX]			= ulm_sband;
    _MPI_BAND.func[_MPI_INT_OP_INDEX]			= ulm_iband;
    _MPI_BAND.func[_MPI_LONG_OP_INDEX]			= ulm_lband;
    _MPI_BAND.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucband;
    _MPI_BAND.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_usband;
    _MPI_BAND.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uiband;
    _MPI_BAND.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulband;
    _MPI_BAND.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_BAND.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_BYTE_OP_INDEX]			= ulm_cband;
    _MPI_BAND.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llband;
    _MPI_BAND.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_BAND.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_BAND.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_BAND.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_BAND.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BAND.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_BOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_BOR.func == NULL) {
	return -1;
    }
    _MPI_BOR.func[_MPI_CHAR_OP_INDEX]			= ulm_cbor;
    _MPI_BOR.func[_MPI_SHORT_OP_INDEX]			= ulm_sbor;
    _MPI_BOR.func[_MPI_INT_OP_INDEX]			= ulm_ibor;
    _MPI_BOR.func[_MPI_LONG_OP_INDEX]			= ulm_lbor;
    _MPI_BOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucbor;
    _MPI_BOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]		= ulm_usbor;
    _MPI_BOR.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uibor;
    _MPI_BOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulbor;
    _MPI_BOR.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_BOR.func[_MPI_DOUBLE_OP_INDEX]			= ulm_null;
    _MPI_BOR.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_PACKED_OP_INDEX]			= ulm_null;
    _MPI_BOR.func[_MPI_BYTE_OP_INDEX]			= ulm_cbor;
    _MPI_BOR.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llbor;
    _MPI_BOR.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_BOR.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_BOR.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_BOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_BOR.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BOR.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_BXOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_BXOR.func == NULL) {
	return -1;
    }
    _MPI_BXOR.func[_MPI_CHAR_OP_INDEX]			= ulm_cbxor;
    _MPI_BXOR.func[_MPI_SHORT_OP_INDEX]			= ulm_sbxor;
    _MPI_BXOR.func[_MPI_INT_OP_INDEX]			= ulm_ibxor;
    _MPI_BXOR.func[_MPI_LONG_OP_INDEX]			= ulm_lbxor;
    _MPI_BXOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucbxor;
    _MPI_BXOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_usbxor;
    _MPI_BXOR.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uibxor;
    _MPI_BXOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulbxor;
    _MPI_BXOR.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_BXOR.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_BYTE_OP_INDEX]			= ulm_cbxor;
    _MPI_BXOR.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llbxor;
    _MPI_BXOR.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_BXOR.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_BXOR.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_BXOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_BXOR.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_BXOR.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_LAND.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_LAND.func == NULL) {
	return -1;
    }
    _MPI_LAND.func[_MPI_CHAR_OP_INDEX]			= ulm_cland;
    _MPI_LAND.func[_MPI_SHORT_OP_INDEX]			= ulm_sland;
    _MPI_LAND.func[_MPI_INT_OP_INDEX]			= ulm_iland;
    _MPI_LAND.func[_MPI_LONG_OP_INDEX]			= ulm_lland;
    _MPI_LAND.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_ucland;
    _MPI_LAND.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_usland;
    _MPI_LAND.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uiland;
    _MPI_LAND.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ulland;
    _MPI_LAND.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_LAND.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_BYTE_OP_INDEX]			= ulm_cland;
    _MPI_LAND.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_llland;
    _MPI_LAND.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_LAND.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_LAND.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_LAND.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_LAND.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LAND.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_LOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_LOR.func == NULL) {
	return -1;
    }
    _MPI_LOR.func[_MPI_CHAR_OP_INDEX]			= ulm_clor;
    _MPI_LOR.func[_MPI_SHORT_OP_INDEX]			= ulm_slor;
    _MPI_LOR.func[_MPI_INT_OP_INDEX]			= ulm_ilor;
    _MPI_LOR.func[_MPI_LONG_OP_INDEX]			= ulm_llor;
    _MPI_LOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_uclor;
    _MPI_LOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]		= ulm_uslor;
    _MPI_LOR.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uilor;
    _MPI_LOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ullor;
    _MPI_LOR.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_LOR.func[_MPI_DOUBLE_OP_INDEX]			= ulm_null;
    _MPI_LOR.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_PACKED_OP_INDEX]			= ulm_null;
    _MPI_LOR.func[_MPI_BYTE_OP_INDEX]			= ulm_clor;
    _MPI_LOR.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_lllor;
    _MPI_LOR.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_LOR.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_LOR.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_LOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_LOR.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LOR.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    _MPI_LXOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (_MPI_LXOR.func == NULL) {
	return -1;
    }
    _MPI_LXOR.func[_MPI_CHAR_OP_INDEX]			= ulm_clxor;
    _MPI_LXOR.func[_MPI_SHORT_OP_INDEX]			= ulm_slxor;
    _MPI_LXOR.func[_MPI_INT_OP_INDEX]			= ulm_ilxor;
    _MPI_LXOR.func[_MPI_LONG_OP_INDEX]			= ulm_llxor;
    _MPI_LXOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]		= ulm_uclxor;
    _MPI_LXOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]	= ulm_uslxor;
    _MPI_LXOR.func[_MPI_UNSIGNED_OP_INDEX]		= ulm_uilxor;
    _MPI_LXOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]		= ulm_ullxor;
    _MPI_LXOR.func[_MPI_FLOAT_OP_INDEX]			= ulm_null;
    _MPI_LXOR.func[_MPI_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_LONG_DOUBLE_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_PACKED_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_BYTE_OP_INDEX]			= ulm_clxor;
    _MPI_LXOR.func[_MPI_LONG_LONG_OP_INDEX]		= ulm_lllxor;
    _MPI_LXOR.func[_MPI_FLOAT_INT_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_DOUBLE_INT_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_LONG_INT_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_2INT_OP_INDEX]			= ulm_null;
    _MPI_LXOR.func[_MPI_SHORT_INT_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]	= ulm_null;
    _MPI_LXOR.func[_MPI_2INTEGER_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_2REAL_OP_INDEX]			= ulm_null;
    _MPI_LXOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]	= ulm_null;
    _MPI_LXOR.func[_MPI_SCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_DCOMPLEX_OP_INDEX]		= ulm_null;
    _MPI_LXOR.func[_MPI_QCOMPLEX_OP_INDEX]		= ulm_null;

    return 0;
}


/*
 * Translate MPI operator and datatype to a function pointer
 */
ULMFunc_t *_mpi_get_reduction_function(MPI_Op mop, MPI_Datatype mtype)
{
    ULMType_t *type = mtype;
    ULMOp_t *op = mop;

    if (op->isbasic) {

	/*
	 * Pre-defined operation: each op has a vector of function
	 * pointers corresponding to the basic types
	 */

	if (type->isbasic == 0) {
	    return NULL;
	}

	return op->func[type->op_index];

    } else {

	/*
	 * User defined function
	 */

	return op->func[0];
    }
}
