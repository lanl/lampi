// Option processing for LA-MPI using popt

#include <popt.h>

#include "option.h"

// helper function prototypes

static void AppendArgvArray(poptContext, int *, int **, const char ****);
static void ChangeDelimiter(char *string, char old_delim, char new_delim);
static void MakeArgv(poptContext con, int *pargc, const char ***pargv);
static void MakeIntegerArray(int argc, const char *argv[], int **array);
static void PrintArgv(const char *name, const char *argv[]);
static void PrintIntegerArray(const char *name, int n, int array[]);


// parse options: if we fail here, exit since there is no hope of recovery

void Option_t::Parse(int argc, char *argv[])
{
    int c;
    poptContext con;

    // popt return values for options requiring special handling

    enum {
        O_NPROCS = 1,
        O_HOST,
        O_CMD,
        O_PATH,
        O_WDIR
    };

    // popt table defining options
    struct poptOption poptTable[] = {
        {"cmd", 0, POPT_ARG_STRING, 0, O_CMD, "A command line to execute (used when different commands are executed on each host)", "\"PER-HOST COMMAND\""},
        {"crc", 0, POPT_ARG_NONE, &crc_m, 0, "Enable use of CRC", ""},
        {"host", 'H', POPT_ARG_STRING, 0, O_HOST, "Comma-delimited list of host names to use", "HOSTLIST"},
        {"mpirun-host", 's', POPT_ARG_STRING, &mpirunhost_m, 0, "Preferred IP interface name/address for TCP/IP administrative and UDP data traffic", "MPIRUNHOST"},
        {"nhosts", 'N', POPT_ARG_INT, &nhosts_m, 0, "Number of hosts", "NHOSTS"},
        {"no-argument-check", 'f', POPT_ARG_NONE|POPT_ARGFLAG_XOR, &argcheck_m, 0, "Disable argument checking", ""},
        {"nprocs", 'n', POPT_ARG_STRING, 0, O_NPROCS, "Number of processes, or a comma-delimited list of the number processes per host", "NPROCS|NPROCLIST"},
        {"path", 'P', POPT_ARG_STRING, 0, O_PATH, "Comma-delimited list of network paths (devices) to use", "PATHLIST"},
        {"tag-output", 't', POPT_ARG_NONE, &tagoutput_m, 0, "Enable tagging of standard output and error", ""},
        {"threads", 'T', POPT_ARG_NONE, &usethreads_m, 0, "Enable thread safety", ""},
        {"verbose", 0, POPT_ARG_NONE, &verbose_m, 0, "Enable verbose output", ""},
        {"version", 'v', POPT_ARG_NONE, &version_m, 0, "Print version and exit", ""},
        {"wdir", 'D', POPT_ARG_STRING, 0, O_WDIR, "Comma-delimited list of working directories for spawned processes", "DIRLIST"},

#ifdef ENABLE_UDP
        {"conf-udp-ack", 0, POPT_ARG_INT, &(udp.doack_m), 0, "Set Quadrics ACK behavior", "0|1"},
        {"conf-upd-checksum", 0, POPT_ARG_INT, &(udp.dochecksum_m), 0, "Set Quadrics checksum behavior", "0|1"},
#endif

#ifdef ENABLE_QSNET
        {"conf-quadrics-ack", 0, POPT_ARG_INT, &(quadrics.doack_m), 0, "Set Quadrics ACK behavior", "0|1"},
        {"conf-quadrics-checksum", 0, POPT_ARG_INT, &(quadrics.dochecksum_m), 0, "Set Quadrics checksum behavior", "0|1"},
#endif

#ifdef ENABLE_GM
        {"conf-gm-doack", 0, POPT_ARG_INT, &(gm.doack_m), 0, "Set GM checksum behavior", "0|1"},
        {"conf-gm-dochecksum", 0, POPT_ARG_INT, &(gm.dochecksum_m), 0, "Set GM checksum behavior", "0|1"},
#endif

        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };

    // create popt option parsing context

    sprintf(argv[0], "mpirun");
    con = poptGetContext(NULL, argc, (const char **) argv, poptTable, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(con, "[OPTIONS]* COMMAND");
    if (argc < 2) {
        poptPrintUsage(con, stderr, 0);
        exit(EXIT_FAILURE);
    }


    // parse options, and handle those which need special processing

    while ((c = poptGetNextOpt(con)) >= 0) {
        switch (c) {
        case O_NPROCS:
            MakeArgv(con, &nprocs_argc_m, &nprocs_argv_m);
            MakeIntegerArray(nprocs_argc_m, nprocs_argv_m, &nprocs_iargv_m);
            break;
        case O_CMD:
            AppendArgvArray(con, &ncmds_m, &cmd_argc_array_m, &cmd_argv_array_m);
            break;
        case O_HOST:
            MakeArgv(con, &host_argc_m, &host_argv_m);
            break;
        case O_PATH:
            MakeArgv(con, &path_argc_m, &path_argv_m);
            break;
        case O_WDIR:
            MakeArgv(con, &wdir_argc_m, &wdir_argv_m);
            break;
        default:
            // unknown option
            fprintf(stderr, "%s: %s: %s\n",
                    argv[0],
                    poptBadOption(con, POPT_BADOPTION_NOALIAS),
                    poptStrerror(c));
            poptPrintUsage(con, stderr, 0);
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (c < -1) {
        // option processing error
        fprintf(stderr, "%s: %s: %s\n",
                argv[0],
                poptBadOption(con, POPT_BADOPTION_NOALIAS),
                poptStrerror(c));
        poptPrintUsage(con, stderr, 0);
        exit(EXIT_FAILURE);
    }

    argv_m = poptGetArgs(con);

    if (ncmds_m > 0 && argv_m) {
        fprintf(stderr, "%s: with --cmd option, COMMAND should not be specified\n",
                argv[0]);
        poptPrintUsage(con, stderr, 0);
        exit(EXIT_FAILURE);
    }

    if (verbose_m) {
        Print();
    }
}


// print options

void Option_t::Print(void)
{
    char name[64];

    fprintf(stderr, "mpirun: Active options:\n");
    fprintf(stderr, "\tmpirunhost = %s\n", mpirunhost_m);
    fprintf(stderr, "\targcheck = %d\n", argcheck_m);
    fprintf(stderr, "\tcrc = %d\n", crc_m);
    fprintf(stderr, "\tnhosts = %d\n", nhosts_m);
    fprintf(stderr, "\ttagoutput = %d\n", tagoutput_m);
    fprintf(stderr, "\tusethreads = %d\n", usethreads_m);

    PrintArgv("argv", argv_m);
    PrintArgv("nprocs_argv", nprocs_argv_m);
    PrintIntegerArray("nprocs_iargv", nprocs_argc_m, nprocs_iargv_m);
    fprintf(stderr, "\tncmds = %d\n", ncmds_m);
    for (int i = 0; i < ncmds_m; i++) {
        sprintf(name, "cmd_argv_array[%d]", i);
        PrintArgv(name, cmd_argv_array_m[i]);
    }
    PrintArgv("host_argv", host_argv_m);
    PrintArgv("path_argv", path_argv_m);
    PrintArgv("wdir_argv", wdir_argv_m);
}



//--HELPER FUNCTIONS--


// parse a an argument which is a comma-delimited list into an
// null-terminated argument vector

static void MakeArgv(poptContext con, int *pargc, const char ***pargv)
{
    char *arg;
    int rc;

    arg = strdup(poptGetOptArg(con));
    if (!arg) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }
    ChangeDelimiter(arg, ',', ' ');
    if ((rc = poptParseArgvString(arg, pargc, pargv)) < 0) {
        perror("option parsing");
        exit(EXIT_FAILURE);
    }
    free(arg);
}


// allocate and initialize an integer array from a null-terminated
// argument vector

static void MakeIntegerArray(int argc, const char *argv[], int **array)
{
    if (!argv) {
        return;
    }
    *array = (int *) malloc(argc * sizeof(int));
    if (!(*array)) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; argv[i]; i++) {
        (*array)[i] = strtol(argv[i], NULL, 10);
    }
}


// grow an array of null-terminated argument vectors by appending the
// argument vector generated from the next popt argument

static void AppendArgvArray(poptContext con,
                            int *pcount,
                            int **pargc_array,
                            const char ****pargv_array)
{
    char *arg;

    (*pcount)++;
    (*pargc_array) = (int *) realloc((*pargc_array),
                                     (*pcount) * sizeof(int));
    if (!(*pargc_array)) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }        

    (*pargv_array) = (const char ***) realloc((*pargv_array),
                                              (*pcount) * sizeof(const char **));
    if (!(*pargv_array)) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }        
    arg = strdup(poptGetOptArg(con));
    if (!arg) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }
    if (poptParseArgvString(arg,
                            &((*pargc_array)[(*pcount) - 1]),
                            &((*pargv_array)[(*pcount) - 1])) < 0) {
        perror("option parsing");
        exit(EXIT_FAILURE);
    }
    free(arg);
}


// change the delimiter character in a list

static void ChangeDelimiter(char *string, char old_delim, char new_delim)
{
    for (char *p = string; *p != '\0'; p++) {
        if (*p == old_delim) {
            *p = new_delim;
        }
    }
}


// print a null-terminated argument vector

static void PrintArgv(const char *name, const char *argv[])
{
    fprintf(stderr, "\t%s:", name);
    if (argv) {
        for (int i = 0; argv[i]; i++) {
            fprintf(stderr, " %d=%s", i, argv[i]);
        }
    }
    fprintf(stderr, "\n");
}


// print an integer array

static void PrintIntegerArray(const char *name, int n, int array[])
{
    fprintf(stderr, "\t%s:", name);
    if (array) {
        for (int i = 0; i < n; i++) {
            fprintf(stderr, " %d=%d", i, array[i]);
        }
    }
    fprintf(stderr, "\n");
}
