/* 
 *   $Id$    
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpio.h"
#include "adio.h"





#if defined(HAVE_WEAK_SYMBOLS) && defined(FORTRANUNDERSCORE) 

void pmpi_file_seek_shared(void);
void mpi_file_seek_shared(void);
/*  void pmpi_file_seek_shared_(void);   this is the real function, below */
void mpi_file_seek_shared_(void);   
void pmpi_file_seek_shared__(void);
void mpi_file_seek_shared__(void);
void PMPI_FILE_SEEK_SHARED(void);
void MPI_FILE_SEEK_SHARED(void);

#pragma weak PMPI_FILE_SEEK_SHARED = pmpi_file_seek_shared_     
#pragma weak pmpi_file_seek_shared = pmpi_file_seek_shared_
#pragma weak pmpi_file_seek_shared__ = pmpi_file_seek_shared_
#pragma weak MPI_FILE_SEEK_SHARED = pmpi_file_seek_shared_     
#pragma weak mpi_file_seek_shared = pmpi_file_seek_shared_
#pragma weak mpi_file_seek_shared_ = pmpi_file_seek_shared_ 
#pragma weak mpi_file_seek_shared__ = pmpi_file_seek_shared_
#endif



#if defined(MPIO_BUILD_PROFILING) || defined(HAVE_WEAK_SYMBOLS)

#if defined(HAVE_WEAK_SYMBOLS)
#if defined(HAVE_PRAGMA_WEAK)
#if defined(FORTRANCAPS)
#pragma weak MPI_FILE_SEEK_SHARED = PMPI_FILE_SEEK_SHARED
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma weak mpi_file_seek_shared__ = pmpi_file_seek_shared__
#elif !defined(FORTRANUNDERSCORE)
#pragma weak mpi_file_seek_shared = pmpi_file_seek_shared
#else
//#pragma weak mpi_file_seek_shared_ = pmpi_file_seek_shared_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(FORTRANCAPS)
#pragma _HP_SECONDARY_DEF PMPI_FILE_SEEK_SHARED MPI_FILE_SEEK_SHARED
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_seek_shared__ mpi_file_seek_shared__
#elif !defined(FORTRANUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_seek_shared mpi_file_seek_shared
#else
#pragma _HP_SECONDARY_DEF pmpi_file_seek_shared_ mpi_file_seek_shared_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(FORTRANCAPS)
#pragma _CRI duplicate MPI_FILE_SEEK_SHARED as PMPI_FILE_SEEK_SHARED
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _CRI duplicate mpi_file_seek_shared__ as pmpi_file_seek_shared__
#elif !defined(FORTRANUNDERSCORE)
#pragma _CRI duplicate mpi_file_seek_shared as pmpi_file_seek_shared
#else
#pragma _CRI duplicate mpi_file_seek_shared_ as pmpi_file_seek_shared_
#endif

/* end of weak pragmas */
#endif
/* Include mapping from MPI->PMPI */
#include "mpioprof.h"
#endif

#ifdef FORTRANCAPS
#define mpi_file_seek_shared_ PMPI_FILE_SEEK_SHARED
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_seek_shared_ pmpi_file_seek_shared__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_seek_shared pmpi_file_seek_shared_
#endif
#define mpi_file_seek_shared_ pmpi_file_seek_shared
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_seek_shared_ pmpi_file_seek_shared
#endif
#define mpi_file_seek_shared_ pmpi_file_seek_shared_
#endif

#else

#ifdef FORTRANCAPS
#define mpi_file_seek_shared_ MPI_FILE_SEEK_SHARED
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_seek_shared_ mpi_file_seek_shared__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_seek_shared mpi_file_seek_shared_
#endif
#define mpi_file_seek_shared_ mpi_file_seek_shared
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_seek_shared_ mpi_file_seek_shared
#endif
#endif
#endif

/* Prototype to keep compiler happy */
FORTRAN_API void FORT_CALL mpi_file_seek_shared_(MPI_Fint *fh,MPI_Offset *offset,int *whence, 
			   int *ierr );

FORTRAN_API void FORT_CALL mpi_file_seek_shared_(MPI_Fint *fh,MPI_Offset *offset,int *whence, int *ierr )
{
    MPI_File fh_c;
    
    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_seek_shared(fh_c,*offset,*whence);
}
