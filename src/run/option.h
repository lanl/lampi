// Option processing for LA-MPI using popt

// A class for parsing and holding command line options.  This is not
// changed after initial argument parsing.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class Option_t {

//--STATE--

public:
    char *mpirunhost_m;
    int argcheck_m;
    int crc_m;
    int nhosts_m;
    int nprocs_m;
    int tagoutput_m;
    int usethreads_m;
    int verbose_m;
    int version_m;

    const char **argv_m;

    int ncmds_m;
    int *cmd_argc_array_m;
    const char ***cmd_argv_array_m;

    int nprocs_argc_m;
    const char **nprocs_argv_m;
    int *nprocs_iargv_m;

    int host_argc_m;
    const char **host_argv_m;

    int path_argc_m;
    const char **path_argv_m;

    int wdir_argc_m;
    const char **wdir_argv_m;

#if ENABLE_UDP
    struct {
        int doack_m;
        int dochecksum_m;
    } udp;
#endif

#if ENABLE_QSNET
    struct {
        int doack_m;
        int dochecksum_m;
    } quadrics;
#endif

#if ENABLE_GM
    struct {
        int doack_m;
        int dochecksum_m;
    } gm;
#endif


//--METHODS--

public:

    Option_t()
        {
            memset(this, 0, sizeof(Option_t));
            argcheck_m = 1;
        }


    ~Option_t()
        {
            Free(mpirunhost_m);
            Free(nprocs_argv_m);
            Free(nprocs_iargv_m);
            for (int i = 0; i < ncmds_m; i++) {
                Free(cmd_argv_array_m[i]);
            }
            Free(cmd_argv_array_m);
            Free(host_argv_m);
            Free(path_argv_m);
            Free(wdir_argv_m);
        }


    // main method: if we fail here, exit since there is no hope of recovery

    void Parse(int argc, char *argv[]);

    void Print(void);

    void Free(void *p)
        {
            if (p) {
                free(p);
            }
        }
};
