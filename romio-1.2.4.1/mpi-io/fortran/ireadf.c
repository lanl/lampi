/* 
 *   $Id$    
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpio.h"
#include "adio.h"





#if defined(HAVE_WEAK_SYMBOLS) && defined(FORTRANUNDERSCORE) 

void pmpi_file_iread(void);
void mpi_file_iread(void);
/*  void pmpi_file_iread_(void);   this is the real function, below */
void mpi_file_iread_(void);   
void pmpi_file_iread__(void);
void mpi_file_iread__(void);
void PMPI_FILE_IREAD(void);
void MPI_FILE_IREAD(void);

#pragma weak PMPI_FILE_IREAD = pmpi_file_iread_     
#pragma weak pmpi_file_iread = pmpi_file_iread_
#pragma weak pmpi_file_iread__ = pmpi_file_iread_
#pragma weak MPI_FILE_IREAD = pmpi_file_iread_     
#pragma weak mpi_file_iread = pmpi_file_iread_
#pragma weak mpi_file_iread_ = pmpi_file_iread_    
#pragma weak mpi_file_iread__ = pmpi_file_iread_
#endif



#if defined(MPIO_BUILD_PROFILING) || defined(HAVE_WEAK_SYMBOLS)
#ifdef FORTRANCAPS
#define mpi_file_iread_ PMPI_FILE_IREAD
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_iread_ pmpi_file_iread__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_iread pmpi_file_iread_
#endif
#define mpi_file_iread_ pmpi_file_iread
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_iread_ pmpi_file_iread
#endif
#define mpi_file_iread_ pmpi_file_iread_
#endif

#if defined(HAVE_WEAK_SYMBOLS)
#if defined(HAVE_PRAGMA_WEAK)
#if defined(FORTRANCAPS)
#pragma weak MPI_FILE_IREAD = PMPI_FILE_IREAD
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma weak mpi_file_iread__ = pmpi_file_iread__
#elif !defined(FORTRANUNDERSCORE)
#pragma weak mpi_file_iread = pmpi_file_iread
#else
//#pragma weak mpi_file_iread_ = pmpi_file_iread_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(FORTRANCAPS)
#pragma _HP_SECONDARY_DEF PMPI_FILE_IREAD MPI_FILE_IREAD
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_iread__ mpi_file_iread__
#elif !defined(FORTRANUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_iread mpi_file_iread
#else
#pragma _HP_SECONDARY_DEF pmpi_file_iread_ mpi_file_iread_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(FORTRANCAPS)
#pragma _CRI duplicate MPI_FILE_IREAD as PMPI_FILE_IREAD
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _CRI duplicate mpi_file_iread__ as pmpi_file_iread__
#elif !defined(FORTRANUNDERSCORE)
#pragma _CRI duplicate mpi_file_iread as pmpi_file_iread
#else
#pragma _CRI duplicate mpi_file_iread_ as pmpi_file_iread_
#endif

/* end of weak pragmas */
#endif
/* Include mapping from MPI->PMPI */
#include "mpioprof.h"
#endif

#else

#ifdef FORTRANCAPS
#define mpi_file_iread_ MPI_FILE_IREAD
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_iread_ mpi_file_iread__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_iread mpi_file_iread_
#endif
#define mpi_file_iread_ mpi_file_iread
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_iread_ mpi_file_iread
#endif
#endif
#endif

#if defined(MPIHP) || defined(MPILAM) || defined(MPILAMPI)
/* Prototype to keep compiler happy */
void mpi_file_iread_(MPI_Fint *fh,void *buf,int *count,
		     MPI_Fint *datatype,MPI_Fint *request, int *ierr );

void mpi_file_iread_(MPI_Fint *fh,void *buf,int *count,
                   MPI_Fint *datatype,MPI_Fint *request, int *ierr )
{
    MPI_File fh_c;
    MPIO_Request req_c;
    MPI_Datatype datatype_c;
    
    datatype_c = MPI_Type_f2c(*datatype);
    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_iread(fh_c,buf,*count,datatype_c,&req_c);
    *request = MPIO_Request_c2f(req_c);
}
#else
/* Prototype to keep compiler happy */
FORTRAN_API void FORT_CALL mpi_file_iread_(MPI_Fint *fh,void *buf,int *count,
		     MPI_Datatype *datatype,MPI_Fint *request, int *ierr );

FORTRAN_API void FORT_CALL mpi_file_iread_(MPI_Fint *fh,void *buf,int *count,
                   MPI_Datatype *datatype,MPI_Fint *request, int *ierr )
{
    MPI_File fh_c;
    MPIO_Request req_c;
    
    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_iread(fh_c,buf,*count,*datatype,&req_c);
    *request = MPIO_Request_c2f(req_c);
}
#endif