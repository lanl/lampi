/* 
 *   $Id$    
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpio.h"
#include "adio.h"





#if defined(HAVE_WEAK_SYMBOLS) && defined(FORTRANUNDERSCORE) 

void pmpi_file_set_atomicity(void);
void mpi_file_set_atomicity(void);
/*  void pmpi_file_set_atomicity_(void);   this is the real function, below */
void mpi_file_set_atomicity_(void);   
void pmpi_file_set_atomicity__(void);
void mpi_file_set_atomicity__(void);
void PMPI_FILE_SET_ATOMICITY(void);
void MPI_FILE_SET_ATOMICITY(void);

#pragma weak PMPI_FILE_SET_ATOMICITY = pmpi_file_set_atomicity_     
#pragma weak pmpi_file_set_atomicity = pmpi_file_set_atomicity_
#pragma weak pmpi_file_set_atomicity__ = pmpi_file_set_atomicity_
#pragma weak MPI_FILE_SET_ATOMICITY = pmpi_file_set_atomicity_     
#pragma weak mpi_file_set_atomicity = pmpi_file_set_atomicity_
#pragma weak mpi_file_set_atomicity_ = pmpi_file_set_atomicity_
#pragma weak mpi_file_set_atomicity__ = pmpi_file_set_atomicity_
#endif



#if defined(MPIO_BUILD_PROFILING) || defined(HAVE_WEAK_SYMBOLS)

#if defined(HAVE_WEAK_SYMBOLS)
#if defined(HAVE_PRAGMA_WEAK)
#if defined(FORTRANCAPS)
#pragma weak MPI_FILE_SET_ATOMICITY = PMPI_FILE_SET_ATOMICITY
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma weak mpi_file_set_atomicity__ = pmpi_file_set_atomicity__
#elif !defined(FORTRANUNDERSCORE)
#pragma weak mpi_file_set_atomicity = pmpi_file_set_atomicity
#else
//#pragma weak mpi_file_set_atomicity_ = pmpi_file_set_atomicity_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(FORTRANCAPS)
#pragma _HP_SECONDARY_DEF PMPI_FILE_SET_ATOMICITY MPI_FILE_SET_ATOMICITY
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_set_atomicity__ mpi_file_set_atomicity__
#elif !defined(FORTRANUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_set_atomicity mpi_file_set_atomicity
#else
#pragma _HP_SECONDARY_DEF pmpi_file_set_atomicity_ mpi_file_set_atomicity_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(FORTRANCAPS)
#pragma _CRI duplicate MPI_FILE_SET_ATOMICITY as PMPI_FILE_SET_ATOMICITY
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _CRI duplicate mpi_file_set_atomicity__ as pmpi_file_set_atomicity__
#elif !defined(FORTRANUNDERSCORE)
#pragma _CRI duplicate mpi_file_set_atomicity as pmpi_file_set_atomicity
#else
#pragma _CRI duplicate mpi_file_set_atomicity_ as pmpi_file_set_atomicity_
#endif

/* end of weak pragmas */
#endif
/* Include mapping from MPI->PMPI */
#include "mpioprof.h"
#endif

#ifdef FORTRANCAPS
#define mpi_file_set_atomicity_ PMPI_FILE_SET_ATOMICITY
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_set_atomicity_ pmpi_file_set_atomicity__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_set_atomicity pmpi_file_set_atomicity_
#endif
#define mpi_file_set_atomicity_ pmpi_file_set_atomicity
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_set_atomicity_ pmpi_file_set_atomicity
#endif
#define mpi_file_set_atomicity_ pmpi_file_set_atomicity_
#endif

#else

#ifdef FORTRANCAPS
#define mpi_file_set_atomicity_ MPI_FILE_SET_ATOMICITY
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_set_atomicity_ mpi_file_set_atomicity__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_set_atomicity mpi_file_set_atomicity_
#endif
#define mpi_file_set_atomicity_ mpi_file_set_atomicity
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_set_atomicity_ mpi_file_set_atomicity
#endif
#endif
#endif

/* Prototype to keep compiler happy */
FORTRAN_API void FORT_CALL mpi_file_set_atomicity_(MPI_Fint *fh,int *flag, int *ierr );

FORTRAN_API void FORT_CALL mpi_file_set_atomicity_(MPI_Fint *fh,int *flag, int *ierr )
{
    MPI_File fh_c;
    
    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_set_atomicity(fh_c,*flag);
}

