/* 
 *   $Id$    
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpio.h"
#include "adio.h"





#if defined(HAVE_WEAK_SYMBOLS) && defined(FORTRANUNDERSCORE) 

void pmpi_file_get_group(void);
void mpi_file_get_group(void);
/*  void pmpi_file_get_group_(void);   this is the real function, below */
void mpi_file_get_group_(void);   
void pmpi_file_get_group__(void);
void mpi_file_get_group__(void);
void PMPI_FILE_GET_GROUP(void);
void MPI_FILE_GET_GROUP(void);

#pragma weak PMPI_FILE_GET_GROUP = pmpi_file_get_group_     
#pragma weak pmpi_file_get_group = pmpi_file_get_group_
#pragma weak pmpi_file_get_group__ = pmpi_file_get_group_
#pragma weak MPI_FILE_GET_GROUP = pmpi_file_get_group_     
#pragma weak mpi_file_get_group = pmpi_file_get_group_
#pragma weak mpi_file_get_group_ = pmpi_file_get_group_
#pragma weak mpi_file_get_group__ = pmpi_file_get_group_
#endif



#if defined(MPIO_BUILD_PROFILING) || defined(HAVE_WEAK_SYMBOLS)

#if defined(HAVE_WEAK_SYMBOLS)
#if defined(HAVE_PRAGMA_WEAK)
#if defined(FORTRANCAPS)
#pragma weak MPI_FILE_GET_GROUP = PMPI_FILE_GET_GROUP
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma weak mpi_file_get_group__ = pmpi_file_get_group__
#elif !defined(FORTRANUNDERSCORE)
#pragma weak mpi_file_get_group = pmpi_file_get_group
#else
//#pragma weak mpi_file_get_group_ = pmpi_file_get_group_
#endif

#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#if defined(FORTRANCAPS)
#pragma _HP_SECONDARY_DEF PMPI_FILE_GET_GROUP MPI_FILE_GET_GROUP
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_get_group__ mpi_file_get_group__
#elif !defined(FORTRANUNDERSCORE)
#pragma _HP_SECONDARY_DEF pmpi_file_get_group mpi_file_get_group
#else
#pragma _HP_SECONDARY_DEF pmpi_file_get_group_ mpi_file_get_group_
#endif

#elif defined(HAVE_PRAGMA_CRI_DUP)
#if defined(FORTRANCAPS)
#pragma _CRI duplicate MPI_FILE_GET_GROUP as PMPI_FILE_GET_GROUP
#elif defined(FORTRANDOUBLEUNDERSCORE)
#pragma _CRI duplicate mpi_file_get_group__ as pmpi_file_get_group__
#elif !defined(FORTRANUNDERSCORE)
#pragma _CRI duplicate mpi_file_get_group as pmpi_file_get_group
#else
#pragma _CRI duplicate mpi_file_get_group_ as pmpi_file_get_group_
#endif

/* end of weak pragmas */
#endif
/* Include mapping from MPI->PMPI */
#include "mpioprof.h"
#endif

#ifdef FORTRANCAPS
#define mpi_file_get_group_ PMPI_FILE_GET_GROUP
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_get_group_ pmpi_file_get_group__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_get_group pmpi_file_get_group_
#endif
#define mpi_file_get_group_ pmpi_file_get_group
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF pmpi_file_get_group_ pmpi_file_get_group
#endif
#define mpi_file_get_group_ pmpi_file_get_group_
#endif

#else

#ifdef FORTRANCAPS
#define mpi_file_get_group_ MPI_FILE_GET_GROUP
#elif defined(FORTRANDOUBLEUNDERSCORE)
#define mpi_file_get_group_ mpi_file_get_group__
#elif !defined(FORTRANUNDERSCORE)
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_get_group mpi_file_get_group_
#endif
#define mpi_file_get_group_ mpi_file_get_group
#else
#if defined(HPUX) || defined(SPPUX)
#pragma _HP_SECONDARY_DEF mpi_file_get_group_ mpi_file_get_group
#endif
#endif
#endif

#if defined(MPIHP) || defined(MPILAM) || defined(MPILAMPI)
/* Prototype to keep compiler happy */
void mpi_file_get_group_(MPI_Fint *fh,MPI_Fint *group, int *ierr );

void mpi_file_get_group_(MPI_Fint *fh,MPI_Fint *group, int *ierr )
{
    MPI_File fh_c;
    MPI_Group group_c;

    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_get_group(fh_c, &group_c);
    *group = MPI_Group_c2f(group_c);
}
#else
/* Prototype to keep compiler happy */
FORTRAN_API void FORT_CALL mpi_file_get_group_(MPI_Fint *fh,MPI_Group *group, int *ierr );

FORTRAN_API void FORT_CALL mpi_file_get_group_(MPI_Fint *fh,MPI_Group *group, int *ierr )
{
    MPI_File fh_c;
    
    fh_c = MPI_File_f2c(*fh);
    *ierr = MPI_File_get_group(fh_c, group);
}
#endif
