
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "run/Run.h"

/* this routine is used to set up some of mpirun's process
 *   characteristics
 */
void lampirun_init_proc()
{
    /*
     * Reset the max number of file descriptors which may be used by
     * the client daemon
     */
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rl);

    /*
     * Initialize TerminateInitiated to false...
     */
    mpirunSetTerminateInitiated(0);

    /*
     * Install signal handlers
     */
    MPIrunInstallSigHandler();


    /* return */
    return;
}
