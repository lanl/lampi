#define ADIOI_TEST_FILE_HANDLE(fh, myname) \
{if (!(fh)) { \
    error_code = MPIR_Err_setmsg(MPI_ERR_FILE, MPIR_ERR_FILE_NULL, myname, (char *) 0, (char *) 0); \
    return ADIOI_Error(MPI_FILE_NULL, error_code, myname); } \
 else if ((fh)->cookie != ADIOI_FILE_COOKIE) { \
    error_code = MPIR_Err_setmsg(MPI_ERR_FILE, MPIR_ERR_FILE_CORRUPT, myname, (char *) 0, (char *) 0); \
    return ADIOI_Error(MPI_FILE_NULL, error_code, myname); } }

