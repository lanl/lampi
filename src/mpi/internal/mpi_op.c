/*
 * Copyright 2002-2004. The Regents of the University of
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

ULMOp_t ULM_MAX = {NULL, 1, 1, 0};
ULMOp_t ULM_MIN = {NULL, 1, 1, 0};
ULMOp_t ULM_SUM = {NULL, 1, 1, 0};
ULMOp_t ULM_PROD = {NULL, 1, 1, 0};
ULMOp_t ULM_MAXLOC = {NULL, 1, 1, 0};
ULMOp_t ULM_MINLOC = {NULL, 1, 1, 0};
ULMOp_t ULM_BAND = {NULL, 1, 1, 0};
ULMOp_t ULM_BOR = {NULL, 1, 1, 0};
ULMOp_t ULM_BXOR = {NULL, 1, 1, 0};
ULMOp_t ULM_LAND = {NULL, 1, 1, 0};
ULMOp_t ULM_LOR = {NULL, 1, 1, 0};
ULMOp_t ULM_LXOR = {NULL, 1, 1, 0};

/*
 * MPI operation initialization
 *
 * Nothing for it but to be long-winded...
 */
int _mpi_init_operations(void)
{
    ULM_MAX.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_MAX.func == NULL) {
        return -1;
    }
    ULM_MAX.func[_MPI_CHAR_OP_INDEX]                    = ulm_cmax;
    ULM_MAX.func[_MPI_SHORT_OP_INDEX]                   = ulm_smax;
    ULM_MAX.func[_MPI_INT_OP_INDEX]                     = ulm_imax;
    ULM_MAX.func[_MPI_LONG_OP_INDEX]                    = ulm_lmax;
    ULM_MAX.func[_MPI_UNSIGNED_CHAR_OP_INDEX]           = ulm_ucmax;
    ULM_MAX.func[_MPI_UNSIGNED_SHORT_OP_INDEX]          = ulm_usmax;
    ULM_MAX.func[_MPI_UNSIGNED_OP_INDEX]                = ulm_uimax;
    ULM_MAX.func[_MPI_UNSIGNED_LONG_OP_INDEX]           = ulm_ulmax;
    ULM_MAX.func[_MPI_FLOAT_OP_INDEX]                   = ulm_fmax;
    ULM_MAX.func[_MPI_DOUBLE_OP_INDEX]                  = ulm_dmax;
    ULM_MAX.func[_MPI_LONG_DOUBLE_OP_INDEX]             = ulm_qmax;
    ULM_MAX.func[_MPI_PACKED_OP_INDEX]                  = ulm_null;
    ULM_MAX.func[_MPI_BYTE_OP_INDEX]                    = ulm_cmax;
    ULM_MAX.func[_MPI_LONG_LONG_OP_INDEX]               = ulm_llmax;
    ULM_MAX.func[_MPI_FLOAT_INT_OP_INDEX]               = ulm_null;
    ULM_MAX.func[_MPI_DOUBLE_INT_OP_INDEX]              = ulm_null;
    ULM_MAX.func[_MPI_LONG_INT_OP_INDEX]                = ulm_null;
    ULM_MAX.func[_MPI_2INT_OP_INDEX]                    = ulm_null;
    ULM_MAX.func[_MPI_SHORT_INT_OP_INDEX]               = ulm_null;
    ULM_MAX.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_MAX.func[_MPI_2INTEGER_OP_INDEX]                = ulm_null;
    ULM_MAX.func[_MPI_2REAL_OP_INDEX]                   = ulm_null;
    ULM_MAX.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]       = ulm_null;
    ULM_MAX.func[_MPI_SCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_MAX.func[_MPI_DCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_MAX.func[_MPI_QCOMPLEX_OP_INDEX]                = ulm_null;

    ULM_MIN.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_MIN.func == NULL) {
        return -1;
    }
    ULM_MIN.func[_MPI_CHAR_OP_INDEX]                    = ulm_cmin;
    ULM_MIN.func[_MPI_SHORT_OP_INDEX]                   = ulm_smin;
    ULM_MIN.func[_MPI_INT_OP_INDEX]                     = ulm_imin;
    ULM_MIN.func[_MPI_LONG_OP_INDEX]                    = ulm_lmin;
    ULM_MIN.func[_MPI_UNSIGNED_CHAR_OP_INDEX]           = ulm_ucmin;
    ULM_MIN.func[_MPI_UNSIGNED_SHORT_OP_INDEX]          = ulm_usmin;
    ULM_MIN.func[_MPI_UNSIGNED_OP_INDEX]                = ulm_uimin;
    ULM_MIN.func[_MPI_UNSIGNED_LONG_OP_INDEX]           = ulm_ulmin;
    ULM_MIN.func[_MPI_FLOAT_OP_INDEX]                   = ulm_fmin;
    ULM_MIN.func[_MPI_DOUBLE_OP_INDEX]                  = ulm_dmin;
    ULM_MIN.func[_MPI_LONG_DOUBLE_OP_INDEX]             = ulm_qmin;
    ULM_MIN.func[_MPI_PACKED_OP_INDEX]                  = ulm_null;
    ULM_MIN.func[_MPI_BYTE_OP_INDEX]                    = ulm_cmin;
    ULM_MIN.func[_MPI_LONG_LONG_OP_INDEX]               = ulm_llmin;
    ULM_MIN.func[_MPI_FLOAT_INT_OP_INDEX]               = ulm_null;
    ULM_MIN.func[_MPI_DOUBLE_INT_OP_INDEX]              = ulm_null;
    ULM_MIN.func[_MPI_LONG_INT_OP_INDEX]                = ulm_null;
    ULM_MIN.func[_MPI_2INT_OP_INDEX]                    = ulm_null;
    ULM_MIN.func[_MPI_SHORT_INT_OP_INDEX]               = ulm_null;
    ULM_MIN.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_MIN.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_MIN.func[_MPI_2INTEGER_OP_INDEX]                = ulm_null;
    ULM_MIN.func[_MPI_2REAL_OP_INDEX]                   = ulm_null;
    ULM_MIN.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]       = ulm_null;
    ULM_MIN.func[_MPI_SCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_MIN.func[_MPI_DCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_MIN.func[_MPI_QCOMPLEX_OP_INDEX]                = ulm_null;

    ULM_SUM.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_SUM.func == NULL) {
        return -1;
    }
    ULM_SUM.func[_MPI_CHAR_OP_INDEX]                    = ulm_csum;
    ULM_SUM.func[_MPI_SHORT_OP_INDEX]                   = ulm_ssum;
    ULM_SUM.func[_MPI_INT_OP_INDEX]                     = ulm_isum;
    ULM_SUM.func[_MPI_LONG_OP_INDEX]                    = ulm_lsum;
    ULM_SUM.func[_MPI_UNSIGNED_CHAR_OP_INDEX]           = ulm_ucsum;
    ULM_SUM.func[_MPI_UNSIGNED_SHORT_OP_INDEX]          = ulm_ussum;
    ULM_SUM.func[_MPI_UNSIGNED_OP_INDEX]                = ulm_uisum;
    ULM_SUM.func[_MPI_UNSIGNED_LONG_OP_INDEX]           = ulm_ulsum;
    ULM_SUM.func[_MPI_FLOAT_OP_INDEX]                   = ulm_fsum;
    ULM_SUM.func[_MPI_DOUBLE_OP_INDEX]                  = ulm_dsum;
    ULM_SUM.func[_MPI_LONG_DOUBLE_OP_INDEX]             = ulm_qsum;
    ULM_SUM.func[_MPI_PACKED_OP_INDEX]                  = ulm_null;
    ULM_SUM.func[_MPI_BYTE_OP_INDEX]                    = ulm_csum;
    ULM_SUM.func[_MPI_LONG_LONG_OP_INDEX]               = ulm_llsum;
    ULM_SUM.func[_MPI_FLOAT_INT_OP_INDEX]               = ulm_null;
    ULM_SUM.func[_MPI_DOUBLE_INT_OP_INDEX]              = ulm_null;
    ULM_SUM.func[_MPI_LONG_INT_OP_INDEX]                = ulm_null;
    ULM_SUM.func[_MPI_2INT_OP_INDEX]                    = ulm_null;
    ULM_SUM.func[_MPI_SHORT_INT_OP_INDEX]               = ulm_null;
    ULM_SUM.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_SUM.func[_MPI_2INTEGER_OP_INDEX]                = ulm_null;
    ULM_SUM.func[_MPI_2REAL_OP_INDEX]                   = ulm_null;
    ULM_SUM.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]       = ulm_null;
    ULM_SUM.func[_MPI_SCOMPLEX_OP_INDEX]                = ulm_scsum;
    ULM_SUM.func[_MPI_DCOMPLEX_OP_INDEX]                = ulm_dcsum;
    ULM_SUM.func[_MPI_QCOMPLEX_OP_INDEX]                = ulm_qcsum;

    ULM_PROD.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_PROD.func == NULL) {
        return -1;
    }
    ULM_PROD.func[_MPI_CHAR_OP_INDEX]                   = ulm_cprod;
    ULM_PROD.func[_MPI_SHORT_OP_INDEX]                  = ulm_sprod;
    ULM_PROD.func[_MPI_INT_OP_INDEX]                    = ulm_iprod;
    ULM_PROD.func[_MPI_LONG_OP_INDEX]                   = ulm_lprod;
    ULM_PROD.func[_MPI_UNSIGNED_CHAR_OP_INDEX]          = ulm_ucprod;
    ULM_PROD.func[_MPI_UNSIGNED_SHORT_OP_INDEX]         = ulm_usprod;
    ULM_PROD.func[_MPI_UNSIGNED_OP_INDEX]               = ulm_uiprod;
    ULM_PROD.func[_MPI_UNSIGNED_LONG_OP_INDEX]          = ulm_ulprod;
    ULM_PROD.func[_MPI_FLOAT_OP_INDEX]                  = ulm_fprod;
    ULM_PROD.func[_MPI_DOUBLE_OP_INDEX]                 = ulm_dprod;
    ULM_PROD.func[_MPI_LONG_DOUBLE_OP_INDEX]            = ulm_qprod;
    ULM_PROD.func[_MPI_PACKED_OP_INDEX]                 = ulm_null;
    ULM_PROD.func[_MPI_BYTE_OP_INDEX]                   = ulm_cprod;
    ULM_PROD.func[_MPI_LONG_LONG_OP_INDEX]              = ulm_llprod;
    ULM_PROD.func[_MPI_FLOAT_INT_OP_INDEX]              = ulm_null;
    ULM_PROD.func[_MPI_DOUBLE_INT_OP_INDEX]             = ulm_null;
    ULM_PROD.func[_MPI_LONG_INT_OP_INDEX]               = ulm_null;
    ULM_PROD.func[_MPI_2INT_OP_INDEX]                   = ulm_null;
    ULM_PROD.func[_MPI_SHORT_INT_OP_INDEX]              = ulm_null;
    ULM_PROD.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]        = ulm_null;
    ULM_PROD.func[_MPI_2INTEGER_OP_INDEX]               = ulm_null;
    ULM_PROD.func[_MPI_2REAL_OP_INDEX]                  = ulm_null;
    ULM_PROD.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]      = ulm_null;
    ULM_PROD.func[_MPI_SCOMPLEX_OP_INDEX]               = ulm_scprod;
    ULM_PROD.func[_MPI_DCOMPLEX_OP_INDEX]               = ulm_dcprod;
    ULM_PROD.func[_MPI_QCOMPLEX_OP_INDEX]               = ulm_qcprod;

    ULM_MAXLOC.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_MAXLOC.func == NULL) {
        return -1;
    }
    ULM_MAXLOC.func[_MPI_CHAR_OP_INDEX]                 = ulm_null;
    ULM_MAXLOC.func[_MPI_SHORT_OP_INDEX]                = ulm_null;
    ULM_MAXLOC.func[_MPI_INT_OP_INDEX]                  = ulm_null;
    ULM_MAXLOC.func[_MPI_LONG_OP_INDEX]                 = ulm_null;
    ULM_MAXLOC.func[_MPI_UNSIGNED_CHAR_OP_INDEX]        = ulm_null;
    ULM_MAXLOC.func[_MPI_UNSIGNED_SHORT_OP_INDEX]       = ulm_null;
    ULM_MAXLOC.func[_MPI_UNSIGNED_OP_INDEX]             = ulm_null;
    ULM_MAXLOC.func[_MPI_UNSIGNED_LONG_OP_INDEX]        = ulm_null;
    ULM_MAXLOC.func[_MPI_FLOAT_OP_INDEX]                = ulm_null;
    ULM_MAXLOC.func[_MPI_DOUBLE_OP_INDEX]               = ulm_null;
    ULM_MAXLOC.func[_MPI_LONG_DOUBLE_OP_INDEX]          = ulm_null;
    ULM_MAXLOC.func[_MPI_PACKED_OP_INDEX]               = ulm_null;
    ULM_MAXLOC.func[_MPI_BYTE_OP_INDEX]                 = ulm_null;
    ULM_MAXLOC.func[_MPI_LONG_LONG_OP_INDEX]            = ulm_null;
    ULM_MAXLOC.func[_MPI_FLOAT_INT_OP_INDEX]            = ulm_fmaxloc;
    ULM_MAXLOC.func[_MPI_DOUBLE_INT_OP_INDEX]           = ulm_dmaxloc;
    ULM_MAXLOC.func[_MPI_LONG_INT_OP_INDEX]             = ulm_lmaxloc;
    ULM_MAXLOC.func[_MPI_2INT_OP_INDEX]                 = ulm_imaxloc;
    ULM_MAXLOC.func[_MPI_SHORT_INT_OP_INDEX]            = ulm_smaxloc;
    ULM_MAXLOC.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]      = ulm_qmaxloc;
    ULM_MAXLOC.func[_MPI_2INTEGER_OP_INDEX]             = ulm_imaxloc;
    ULM_MAXLOC.func[_MPI_2REAL_OP_INDEX]                = ulm_ffmaxloc;
    ULM_MAXLOC.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]    = ulm_ddmaxloc;
    ULM_MAXLOC.func[_MPI_SCOMPLEX_OP_INDEX]             = ulm_null;
    ULM_MAXLOC.func[_MPI_DCOMPLEX_OP_INDEX]             = ulm_null;
    ULM_MAXLOC.func[_MPI_QCOMPLEX_OP_INDEX]             = ulm_null;

    ULM_MINLOC.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_MINLOC.func == NULL) {
        return -1;
    }
    ULM_MINLOC.func[_MPI_CHAR_OP_INDEX]                 = ulm_null;
    ULM_MINLOC.func[_MPI_SHORT_OP_INDEX]                = ulm_null;
    ULM_MINLOC.func[_MPI_INT_OP_INDEX]                  = ulm_null;
    ULM_MINLOC.func[_MPI_LONG_OP_INDEX]                 = ulm_null;
    ULM_MINLOC.func[_MPI_UNSIGNED_CHAR_OP_INDEX]        = ulm_null;
    ULM_MINLOC.func[_MPI_UNSIGNED_SHORT_OP_INDEX]       = ulm_null;
    ULM_MINLOC.func[_MPI_UNSIGNED_OP_INDEX]             = ulm_null;
    ULM_MINLOC.func[_MPI_UNSIGNED_LONG_OP_INDEX]        = ulm_null;
    ULM_MINLOC.func[_MPI_FLOAT_OP_INDEX]                = ulm_null;
    ULM_MINLOC.func[_MPI_DOUBLE_OP_INDEX]               = ulm_null;
    ULM_MINLOC.func[_MPI_LONG_DOUBLE_OP_INDEX]          = ulm_null;
    ULM_MINLOC.func[_MPI_PACKED_OP_INDEX]               = ulm_null;
    ULM_MINLOC.func[_MPI_BYTE_OP_INDEX]                 = ulm_null;
    ULM_MINLOC.func[_MPI_LONG_LONG_OP_INDEX]            = ulm_null;
    ULM_MINLOC.func[_MPI_FLOAT_INT_OP_INDEX]            = ulm_fminloc;
    ULM_MINLOC.func[_MPI_DOUBLE_INT_OP_INDEX]           = ulm_dminloc;
    ULM_MINLOC.func[_MPI_LONG_INT_OP_INDEX]             = ulm_lminloc;
    ULM_MINLOC.func[_MPI_2INT_OP_INDEX]                 = ulm_iminloc;
    ULM_MINLOC.func[_MPI_SHORT_INT_OP_INDEX]            = ulm_sminloc;
    ULM_MINLOC.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]      = ulm_qminloc;
    ULM_MINLOC.func[_MPI_2INTEGER_OP_INDEX]             = ulm_iminloc;
    ULM_MINLOC.func[_MPI_2REAL_OP_INDEX]                = ulm_ffminloc;
    ULM_MINLOC.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]    = ulm_ddminloc;
    ULM_MINLOC.func[_MPI_SCOMPLEX_OP_INDEX]             = ulm_null;
    ULM_MINLOC.func[_MPI_DCOMPLEX_OP_INDEX]             = ulm_null;
    ULM_MINLOC.func[_MPI_QCOMPLEX_OP_INDEX]             = ulm_null;

    ULM_BAND.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_BAND.func == NULL) {
        return -1;
    }
    ULM_BAND.func[_MPI_CHAR_OP_INDEX]                   = ulm_cband;
    ULM_BAND.func[_MPI_SHORT_OP_INDEX]                  = ulm_sband;
    ULM_BAND.func[_MPI_INT_OP_INDEX]                    = ulm_iband;
    ULM_BAND.func[_MPI_LONG_OP_INDEX]                   = ulm_lband;
    ULM_BAND.func[_MPI_UNSIGNED_CHAR_OP_INDEX]          = ulm_ucband;
    ULM_BAND.func[_MPI_UNSIGNED_SHORT_OP_INDEX]         = ulm_usband;
    ULM_BAND.func[_MPI_UNSIGNED_OP_INDEX]               = ulm_uiband;
    ULM_BAND.func[_MPI_UNSIGNED_LONG_OP_INDEX]          = ulm_ulband;
    ULM_BAND.func[_MPI_FLOAT_OP_INDEX]                  = ulm_null;
    ULM_BAND.func[_MPI_DOUBLE_OP_INDEX]                 = ulm_null;
    ULM_BAND.func[_MPI_LONG_DOUBLE_OP_INDEX]            = ulm_null;
    ULM_BAND.func[_MPI_PACKED_OP_INDEX]                 = ulm_null;
    ULM_BAND.func[_MPI_BYTE_OP_INDEX]                   = ulm_cband;
    ULM_BAND.func[_MPI_LONG_LONG_OP_INDEX]              = ulm_llband;
    ULM_BAND.func[_MPI_FLOAT_INT_OP_INDEX]              = ulm_null;
    ULM_BAND.func[_MPI_DOUBLE_INT_OP_INDEX]             = ulm_null;
    ULM_BAND.func[_MPI_LONG_INT_OP_INDEX]               = ulm_null;
    ULM_BAND.func[_MPI_2INT_OP_INDEX]                   = ulm_null;
    ULM_BAND.func[_MPI_SHORT_INT_OP_INDEX]              = ulm_null;
    ULM_BAND.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]        = ulm_null;
    ULM_BAND.func[_MPI_2INTEGER_OP_INDEX]               = ulm_null;
    ULM_BAND.func[_MPI_2REAL_OP_INDEX]                  = ulm_null;
    ULM_BAND.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]      = ulm_null;
    ULM_BAND.func[_MPI_SCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_BAND.func[_MPI_DCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_BAND.func[_MPI_QCOMPLEX_OP_INDEX]               = ulm_null;

    ULM_BOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_BOR.func == NULL) {
        return -1;
    }
    ULM_BOR.func[_MPI_CHAR_OP_INDEX]                    = ulm_cbor;
    ULM_BOR.func[_MPI_SHORT_OP_INDEX]                   = ulm_sbor;
    ULM_BOR.func[_MPI_INT_OP_INDEX]                     = ulm_ibor;
    ULM_BOR.func[_MPI_LONG_OP_INDEX]                    = ulm_lbor;
    ULM_BOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]           = ulm_ucbor;
    ULM_BOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]          = ulm_usbor;
    ULM_BOR.func[_MPI_UNSIGNED_OP_INDEX]                = ulm_uibor;
    ULM_BOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]           = ulm_ulbor;
    ULM_BOR.func[_MPI_FLOAT_OP_INDEX]                   = ulm_null;
    ULM_BOR.func[_MPI_DOUBLE_OP_INDEX]                  = ulm_null;
    ULM_BOR.func[_MPI_LONG_DOUBLE_OP_INDEX]             = ulm_null;
    ULM_BOR.func[_MPI_PACKED_OP_INDEX]                  = ulm_null;
    ULM_BOR.func[_MPI_BYTE_OP_INDEX]                    = ulm_cbor;
    ULM_BOR.func[_MPI_LONG_LONG_OP_INDEX]               = ulm_llbor;
    ULM_BOR.func[_MPI_FLOAT_INT_OP_INDEX]               = ulm_null;
    ULM_BOR.func[_MPI_DOUBLE_INT_OP_INDEX]              = ulm_null;
    ULM_BOR.func[_MPI_LONG_INT_OP_INDEX]                = ulm_null;
    ULM_BOR.func[_MPI_2INT_OP_INDEX]                    = ulm_null;
    ULM_BOR.func[_MPI_SHORT_INT_OP_INDEX]               = ulm_null;
    ULM_BOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_BOR.func[_MPI_2INTEGER_OP_INDEX]                = ulm_null;
    ULM_BOR.func[_MPI_2REAL_OP_INDEX]                   = ulm_null;
    ULM_BOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]       = ulm_null;
    ULM_BOR.func[_MPI_SCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_BOR.func[_MPI_DCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_BOR.func[_MPI_QCOMPLEX_OP_INDEX]                = ulm_null;

    ULM_BXOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_BXOR.func == NULL) {
        return -1;
    }
    ULM_BXOR.func[_MPI_CHAR_OP_INDEX]                   = ulm_cbxor;
    ULM_BXOR.func[_MPI_SHORT_OP_INDEX]                  = ulm_sbxor;
    ULM_BXOR.func[_MPI_INT_OP_INDEX]                    = ulm_ibxor;
    ULM_BXOR.func[_MPI_LONG_OP_INDEX]                   = ulm_lbxor;
    ULM_BXOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]          = ulm_ucbxor;
    ULM_BXOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]         = ulm_usbxor;
    ULM_BXOR.func[_MPI_UNSIGNED_OP_INDEX]               = ulm_uibxor;
    ULM_BXOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]          = ulm_ulbxor;
    ULM_BXOR.func[_MPI_FLOAT_OP_INDEX]                  = ulm_null;
    ULM_BXOR.func[_MPI_DOUBLE_OP_INDEX]                 = ulm_null;
    ULM_BXOR.func[_MPI_LONG_DOUBLE_OP_INDEX]            = ulm_null;
    ULM_BXOR.func[_MPI_PACKED_OP_INDEX]                 = ulm_null;
    ULM_BXOR.func[_MPI_BYTE_OP_INDEX]                   = ulm_cbxor;
    ULM_BXOR.func[_MPI_LONG_LONG_OP_INDEX]              = ulm_llbxor;
    ULM_BXOR.func[_MPI_FLOAT_INT_OP_INDEX]              = ulm_null;
    ULM_BXOR.func[_MPI_DOUBLE_INT_OP_INDEX]             = ulm_null;
    ULM_BXOR.func[_MPI_LONG_INT_OP_INDEX]               = ulm_null;
    ULM_BXOR.func[_MPI_2INT_OP_INDEX]                   = ulm_null;
    ULM_BXOR.func[_MPI_SHORT_INT_OP_INDEX]              = ulm_null;
    ULM_BXOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]        = ulm_null;
    ULM_BXOR.func[_MPI_2INTEGER_OP_INDEX]               = ulm_null;
    ULM_BXOR.func[_MPI_2REAL_OP_INDEX]                  = ulm_null;
    ULM_BXOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]      = ulm_null;
    ULM_BXOR.func[_MPI_SCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_BXOR.func[_MPI_DCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_BXOR.func[_MPI_QCOMPLEX_OP_INDEX]               = ulm_null;

    ULM_LAND.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_LAND.func == NULL) {
        return -1;
    }
    ULM_LAND.func[_MPI_CHAR_OP_INDEX]                   = ulm_cland;
    ULM_LAND.func[_MPI_SHORT_OP_INDEX]                  = ulm_sland;
    ULM_LAND.func[_MPI_INT_OP_INDEX]                    = ulm_iland;
    ULM_LAND.func[_MPI_LONG_OP_INDEX]                   = ulm_lland;
    ULM_LAND.func[_MPI_UNSIGNED_CHAR_OP_INDEX]          = ulm_ucland;
    ULM_LAND.func[_MPI_UNSIGNED_SHORT_OP_INDEX]         = ulm_usland;
    ULM_LAND.func[_MPI_UNSIGNED_OP_INDEX]               = ulm_uiland;
    ULM_LAND.func[_MPI_UNSIGNED_LONG_OP_INDEX]          = ulm_ulland;
    ULM_LAND.func[_MPI_FLOAT_OP_INDEX]                  = ulm_null;
    ULM_LAND.func[_MPI_DOUBLE_OP_INDEX]                 = ulm_null;
    ULM_LAND.func[_MPI_LONG_DOUBLE_OP_INDEX]            = ulm_null;
    ULM_LAND.func[_MPI_PACKED_OP_INDEX]                 = ulm_null;
    ULM_LAND.func[_MPI_BYTE_OP_INDEX]                   = ulm_cland;
    ULM_LAND.func[_MPI_LONG_LONG_OP_INDEX]              = ulm_llland;
    ULM_LAND.func[_MPI_FLOAT_INT_OP_INDEX]              = ulm_null;
    ULM_LAND.func[_MPI_DOUBLE_INT_OP_INDEX]             = ulm_null;
    ULM_LAND.func[_MPI_LONG_INT_OP_INDEX]               = ulm_null;
    ULM_LAND.func[_MPI_2INT_OP_INDEX]                   = ulm_null;
    ULM_LAND.func[_MPI_SHORT_INT_OP_INDEX]              = ulm_null;
    ULM_LAND.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]        = ulm_null;
    ULM_LAND.func[_MPI_2INTEGER_OP_INDEX]               = ulm_null;
    ULM_LAND.func[_MPI_2REAL_OP_INDEX]                  = ulm_null;
    ULM_LAND.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]      = ulm_null;
    ULM_LAND.func[_MPI_SCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_LAND.func[_MPI_DCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_LAND.func[_MPI_QCOMPLEX_OP_INDEX]               = ulm_null;

    ULM_LOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_LOR.func == NULL) {
        return -1;
    }
    ULM_LOR.func[_MPI_CHAR_OP_INDEX]                    = ulm_clor;
    ULM_LOR.func[_MPI_SHORT_OP_INDEX]                   = ulm_slor;
    ULM_LOR.func[_MPI_INT_OP_INDEX]                     = ulm_ilor;
    ULM_LOR.func[_MPI_LONG_OP_INDEX]                    = ulm_llor;
    ULM_LOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]           = ulm_uclor;
    ULM_LOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]          = ulm_uslor;
    ULM_LOR.func[_MPI_UNSIGNED_OP_INDEX]                = ulm_uilor;
    ULM_LOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]           = ulm_ullor;
    ULM_LOR.func[_MPI_FLOAT_OP_INDEX]                   = ulm_null;
    ULM_LOR.func[_MPI_DOUBLE_OP_INDEX]                  = ulm_null;
    ULM_LOR.func[_MPI_LONG_DOUBLE_OP_INDEX]             = ulm_null;
    ULM_LOR.func[_MPI_PACKED_OP_INDEX]                  = ulm_null;
    ULM_LOR.func[_MPI_BYTE_OP_INDEX]                    = ulm_clor;
    ULM_LOR.func[_MPI_LONG_LONG_OP_INDEX]               = ulm_lllor;
    ULM_LOR.func[_MPI_FLOAT_INT_OP_INDEX]               = ulm_null;
    ULM_LOR.func[_MPI_DOUBLE_INT_OP_INDEX]              = ulm_null;
    ULM_LOR.func[_MPI_LONG_INT_OP_INDEX]                = ulm_null;
    ULM_LOR.func[_MPI_2INT_OP_INDEX]                    = ulm_null;
    ULM_LOR.func[_MPI_SHORT_INT_OP_INDEX]               = ulm_null;
    ULM_LOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]         = ulm_null;
    ULM_LOR.func[_MPI_2INTEGER_OP_INDEX]                = ulm_null;
    ULM_LOR.func[_MPI_2REAL_OP_INDEX]                   = ulm_null;
    ULM_LOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]       = ulm_null;
    ULM_LOR.func[_MPI_SCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_LOR.func[_MPI_DCOMPLEX_OP_INDEX]                = ulm_null;
    ULM_LOR.func[_MPI_QCOMPLEX_OP_INDEX]                = ulm_null;

    ULM_LXOR.func = ulm_malloc(NUMBER_OF_BASIC_TYPES * sizeof(ULMFunc_t *));
    if (ULM_LXOR.func == NULL) {
        return -1;
    }
    ULM_LXOR.func[_MPI_CHAR_OP_INDEX]                   = ulm_clxor;
    ULM_LXOR.func[_MPI_SHORT_OP_INDEX]                  = ulm_slxor;
    ULM_LXOR.func[_MPI_INT_OP_INDEX]                    = ulm_ilxor;
    ULM_LXOR.func[_MPI_LONG_OP_INDEX]                   = ulm_llxor;
    ULM_LXOR.func[_MPI_UNSIGNED_CHAR_OP_INDEX]          = ulm_uclxor;
    ULM_LXOR.func[_MPI_UNSIGNED_SHORT_OP_INDEX]         = ulm_uslxor;
    ULM_LXOR.func[_MPI_UNSIGNED_OP_INDEX]               = ulm_uilxor;
    ULM_LXOR.func[_MPI_UNSIGNED_LONG_OP_INDEX]          = ulm_ullxor;
    ULM_LXOR.func[_MPI_FLOAT_OP_INDEX]                  = ulm_null;
    ULM_LXOR.func[_MPI_DOUBLE_OP_INDEX]                 = ulm_null;
    ULM_LXOR.func[_MPI_LONG_DOUBLE_OP_INDEX]            = ulm_null;
    ULM_LXOR.func[_MPI_PACKED_OP_INDEX]                 = ulm_null;
    ULM_LXOR.func[_MPI_BYTE_OP_INDEX]                   = ulm_clxor;
    ULM_LXOR.func[_MPI_LONG_LONG_OP_INDEX]              = ulm_lllxor;
    ULM_LXOR.func[_MPI_FLOAT_INT_OP_INDEX]              = ulm_null;
    ULM_LXOR.func[_MPI_DOUBLE_INT_OP_INDEX]             = ulm_null;
    ULM_LXOR.func[_MPI_LONG_INT_OP_INDEX]               = ulm_null;
    ULM_LXOR.func[_MPI_2INT_OP_INDEX]                   = ulm_null;
    ULM_LXOR.func[_MPI_SHORT_INT_OP_INDEX]              = ulm_null;
    ULM_LXOR.func[_MPI_LONG_DOUBLE_INT_OP_INDEX]        = ulm_null;
    ULM_LXOR.func[_MPI_2INTEGER_OP_INDEX]               = ulm_null;
    ULM_LXOR.func[_MPI_2REAL_OP_INDEX]                  = ulm_null;
    ULM_LXOR.func[_MPI_2DOUBLE_PRECISION_OP_INDEX]      = ulm_null;
    ULM_LXOR.func[_MPI_SCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_LXOR.func[_MPI_DCOMPLEX_OP_INDEX]               = ulm_null;
    ULM_LXOR.func[_MPI_QCOMPLEX_OP_INDEX]               = ulm_null;

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
