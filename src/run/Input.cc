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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Misc functions needed for reading in input data
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "util/ParseString.h"
#include "run/Input.h"
#include "run/Run.h"


Options_t Options[] =
{
    {"-jobfile",
     "JobDescriptionFile",
     STRING_ARGS,
     NoOpFunction,
     NotImplementedFunction,
     "File containing parameter settings", 0, "\0"
    },
    {"-nolsf",
     "NoLSF",
     NO_ARGS,
     NoOpFunction,
     setNoLSF,
     "No LSF control will be used in job", 0, "\0"
    },
    /* HostCount must be before HostList */
    {"-N",
     "HostCount",
     STRING_ARGS,
     NoOpFunction,
     GetAppHostCount,
     "Number of hosts (machines)", 0, "\0"
    },
    /* HostList must be before Procs */
    {"-H",
     "HostList",
     STRING_ARGS,
     NoOpFunction,
     GetAppHostData,
     "List of hosts (machines)", 0, "\0"
    },
    {"-machinefile",
     "MachineFile",
     STRING_ARGS,
     NoOpFunction,
     GetAppHostDataFromMachineFile,
     "File containing list of hosts (machines)", 0, "\0"
    },
    /* Procs must be after HostCount and HostList */
    {"-np",
     "Procs",
     STRING_ARGS,
     NoOpFunction,
     GetClientProcessCount,
     "Number of processes total or on each host (machine)", 0, "\0"
    },
    /* alias for -np, call ...NoInput here! */
    {"-n",
     "ProcsAlias",
     STRING_ARGS,
     GetClientProcessCountNoInput,
     GetClientProcessCount,
     "Number of processes total or on each host (machine)", 0, "\0"
    },
    {"-s",
     "MpirunHostname",
     STRING_ARGS,
     GetMpirunHostnameNoInput,
     GetMpirunHostname,
     "IP hostname or address of mpirun host", 0, "\0"
    },
    {"-i",
     "InterfaceList",
     STRING_ARGS,
     GetInterfaceNoInput,
     GetInterfaceList,
     "List of interface names to be used by TCP/UDP paths", 0, "\0"
    },
    {"-ni",
     "Interfaces",
     STRING_ARGS,
     NoOpFunction,
     GetInterfaceCount,
     "Maximum number of interfaces to be used by TCP/UDP paths", 0, "\0"
    },
    {"-cpulist",
     "CpuList",
     STRING_ARGS,
     NoOpFunction,
     GetClientCpus,
     "Which CPUs to use", 0, "\0"
    },
    {"-d",
     "WorkingDir",
     STRING_ARGS,
     GetClientWorkingDirectoryNoInp,
     GetClientWorkingDirectory,
     "Current directory", 0, "\0"
    },
    {"-f",
     "NoArgCheck",
     NO_ARGS,
     NoOpFunction,
     SetCheckArgsFalse,
     "No checking of MPI arguments", 0, "\0"
    },
    {"-t",
     "OutputPrefix",
     NO_ARGS,
     NoOpFunction,
     SetOutputPrefixTrue,
     "Prefix standard output and error with helpful information", 0, "\0"
    },
    {"-q",
     "Quiet",
     NO_ARGS,
     NoOpFunction,
     SetQuietTrue,
     "Suppress start-up messages", 0, "\0"
    },
    {"-v",
     "Verbose",
     NO_ARGS,
     NoOpFunction,
     SetVerboseTrue,
     "Verbose start-up messages", 0, "\0"
    },
    {"-dapp",
     "DirectoryOfBinary",
     STRING_ARGS,
     NoOpFunction,
     GetAppDir,
     "Absolute path to binary", 0, "0"
    },
    {"-exe",
     "BinaryName",
     STRING_ARGS,
     FatalNoInput,
     GetClientApp,
     "Binary name", 0, "0"
    },
    {"-dev",
     "NetworkDevices",
     STRING_ARGS,
     GetNetworkDevListNoInput,
     GetNetworkDevList,
     "Type of network", 0, "\0"
    },
    {"-tvdbgstrt",
     "TotalViewDebugStartup",
     NO_ARGS,
     NoOpFunction,
     GetTVDaemon,
     "Debug the ULM library", 0, "\0"
    },
    {"-gdb",
     "GDBDebugStartup",
     NO_ARGS,
     NoOpFunction,
     GetGDB,
     "Debug LA-MPI library with gdb and xterm", 0, "\0"
    },
    {"-mh",
     "MasterHost",
     STRING_ARGS,
     NoOpFunction,
     RearrangeHostList,
     "Startup host", 0, "\0"
    },
    {"-env", "EnvVars",
     STRING_ARGS,
     NoOpFunction,
     parseEnvironmentSettings,
     "List environment variables and settings", 0, "\0"
    },
    {"-norsrccontinue",
     "OutOfResourceContinue",
     NO_ARGS,
     NoOpFunction,
     parseOutOfResourceContinue,
     "  ", 1, "\0"
    },
    {"-retry",
     "MaxRetryWhenNoResource",
     STRING_ARGS,
     NoOpFunction,
     parseMaxRetries,
     "Number of times to retry for resources", 0, "0"
    },
    {"-local",
     "Local",
     NO_ARGS,
     NoOpFunction,
     setLocal,
     "Run on the local host using fork/exec to spawn the processes", 0, "0"
    },
    {"-ssh",
     "UseSSH",
     NO_ARGS,
     NoOpFunction,
     setUseSSH,
     "use SSH instead of RSH to create remote processes", 0, "0"
    },
    {"-threads",
        "NoThreads",
        NO_ARGS,
        NoOpFunction,
        setThreads,
        "Threads used in job", 0, "0"
    },
    {"-minsmprecvdesc",
     "MinSMPrecvdescpages",
     STRING_ARGS,
     NoOpFunction,
     parseMinSMPRecvDescPages,
     "Min shared memory recv descriptor pages", 0, "\0"
    },
    {"-maxsmprecvdesc",
     "MaxSMPrecvdescpages",
     STRING_ARGS,
     NoOpFunction,
     parseMaxSMPRecvDescPages,
     "Max shared memory recv descriptor pages", 0, "\0"
    },
    {"-totsmprecvdesc",
     "TotSMPrecvdescpages",
     STRING_ARGS,
     NoOpFunction,
     parseTotalSMPRecvDescPages,
     "Total shared memory recv descriptor pages", 0, "\0"
    },
    {"-minsmpisenddesc",
     "MinSMPisenddescpages",
     STRING_ARGS,
     NoOpFunction,
     parseMinSMPISendDescPages,
     "Min shared memory isend descriptor pages", 0, "\0"
    },
    {"-maxsmpisenddesc",
     "MaxSMPisenddescpages",
     STRING_ARGS,
     NoOpFunction,
     parseMaxSMPISendDescPages,
     "Max shared memory isend descriptor pages", 0, "\0"
    },
    {"-totsmpisenddesc",
     "TotSMPisenddescpages",
     STRING_ARGS,
     NoOpFunction,
     parseTotalSMPISendDescPages,
     "Total shared memory isend descriptor pages", 0, "\0"
    },
    {"-totirecvdesc",
     "TotIrecvdescpages",
     STRING_ARGS,
     NoOpFunction,
     parseTotalIRecvDescPages,
     "Total irevc descriptor Pages", 0, "\0"
    },
    {"-totsmpdatapgs",
     "TotSMPDatapages",
     STRING_ARGS,
     NoOpFunction,
     parseTotalSMPDataPages,
     "Total shared memory data pages", 0, "\0"
    },
    {"-minsmpdatapgs",
     "MinSMPDatapages",
     STRING_ARGS,
     NoOpFunction,
     parseMinSMPDataPages,
     "Min shared memory data pages", 0, "\0"
    },
    {"-maxsmpdatapgs",
     "MaxSMPDatapages",
     STRING_ARGS,
     NoOpFunction,
     parseMaxSMPDataPages,
     "Max shared memory data pages", 0, "\0"
    },
#if ENABLE_NUMA
    {"-cpuspernode", "CpusPerNode",
     STRING_ARGS, NoOpFunction, parseCpusPerNode, "Number of CPU's per node to use", 0, "\0"},
    {"-resrcaffinity", "ResourceAffinity",
     STRING_ARGS, NoOpFunction, parseResourceAffinity, "Resource affinity usage", 0, "\0"},
    {"-defltaffinity", "DefaultAffinity",
     STRING_ARGS, NoOpFunction, parseDefaultAffinity, "Resoruce affinity specification", 0, "\0"},
    {"-mandatoryAffinity", "MandatoryAffinity",
     STRING_ARGS, NoOpFunction, parseMandatoryAffinity, "Resource affinity usage mandatory", 0, "\0"},
#endif				// ENABLE_NUMA
    {"-maxcomms",
     "MaxComms",
     STRING_ARGS,
     NoOpFunction,
     parseMaxComms,
     "Maximum number of communicators on each host", 0, "\0"
    },
    {"-smdescmemperproc",
     "SMDescMemPerProc",
     STRING_ARGS,
     NoOpFunction,
     parseSMDescMemPerProc,
     "Descriptor Shared Memory pool per process allocation - in bytes", 0, "\0"
    },
    {"-qr",
     "QuadricsRails",
     STRING_ARGS,
     NoOpFunction,
     parseQuadricsRails,
     "Quadrics rail numbers to use", 0, "\0"
    },
    {"-crc",
     "UseCRC",
     NO_ARGS,
     NoOpFunction,
     parseUseCRC,
     "Use CRCs instead of checksums", 0, "\0"
    },
    {"-qf",
     "QuadricsFlags",
     STRING_ARGS,
     NoOpFunction,
     parseQuadricsFlags,
     "Special Quadrics flags -- noack, ack, nochecksum, checksum", 0, "\0"
    },
    {"-mf",
     "MyrinetFlags",
     STRING_ARGS,
     NoOpFunction,
     parseMyrinetFlags,
     "Special Myrinet GM flags -- noack, ack, nochecksum, checksum, <number> (fragment size)", 0, "\0"
    },
    {"-if",
     "IBFlags",
     STRING_ARGS,
     NoOpFunction,
     parseIBFlags,
     "Special InfiniBand flags -- noack, ack, nochecksum, checksum", 0, "\0"
    }

#if ENABLE_TCP
    ,
    {"-tcpmaxfrag",
     "TCPMaxFragment",
     STRING_ARGS,
     NoOpFunction,
     parseTCPMaxFragment,
     "TCP maximum fragment size", 0, "\0"
    },
    {"-tcpeagersend",
     "TCPEagerSend",
     STRING_ARGS,
     NoOpFunction,
     parseTCPEagerSend,
     "TCP eager send size", 0, "\0"
    },
    {"-tcpconretries",
     "TCPConnectRetries",
     STRING_ARGS,
     NoOpFunction,
     parseTCPConnectRetries,
     "TCP connection retries", 0, "\0"
    }
#endif
};

int SizeOfOptions = sizeof(Options) / sizeof(Options_t);

void Usage(FILE *stream)
{
    fprintf(stream,
            "Usage:\n"
            "    mpirun [OPTION]... EXECUTABLE [ARGUMENT]...\n"
            " or mpirun -h|-help\n"
            " or mpirun -version\n"
            " or mpirun -list-options\n"
            "\n"
            "Run a job under the LA-MPI message-passing system\n"
            "\n"
            "Options\n"
            "-n|-np NPROCS     Number of processes. A single value specifies the\n"
            "                  total number of processes, while a comma-delimited\n"
            "                  list of numbers specifies the number of processes\n"
            "                  on each host.\n"
            "-N NHOSTS         Number of hosts.\n"
            "-H HOSTLIST       A comma-delimited list of hosts (if applicable).\n"
            "-crc              Use 32-bit CRCs instead of additive checksums if applicable.\n"
            "-d DIRLIST        A comma-delimited list of one or more working directories\n"
            "                  for spawned processes.\n"
            "-dapp DIRLIST     A comma-delimited list of one or more directories\n"
            "                  where the executable is located.\n"
            "-dev PATHLIST     A colon-delimited list of one or more network paths to use.\n" 
            "-i IFLIST         A comma-delimited list of one or more interfaces names\n"
            "                  to be used by TCP/UDP paths.\n"
            "-local            Run on the local host using fork/exec to spawn processes.\n"
            "-ni NINTERFACES   Maximum number of interfaces to be used by TCP/UDP paths.\n"
            "-q                Suppress start-up messages.\n"
            "-s MPIRUNHOST     A comma-delimited list of preferred IP interface name\n"
            "                  fragments (whole, suffix, or prefix) or addresses for TCP/IP\n"
            "                  administrative and UDP/IP data traffic.\n"
            "-ssh              Use ssh rather than the default rsh if no other start-up\n"
            "                  mechanism is available.\n"
            "-t                Tag standard output/error with source information.\n"
            "-v                Verbose start-up information.\n"
            "-threads          Enable thread safety.\n"
            "-qf FLAGSLIST     A comma-delimited list of keywords for operation on\n"
            "                  Quadrics networks.  The keywords supported are \"ack\",\n"
            "                  \"noack\", \"checksum\", and \"nochecksum\"\n"
            "                  [THIS OPTION MAY CHANGE IN FUTURE RELEASES].\n"
            "-mf FLAGSLIST     A comma-delimited list of keywords for operation on\n"
            "                  Myrinet GM networks.  The keywords supported are \"ack\",\n"
            "                  \"noack\", \"checksum\", and \"nochecksum\"\n"
            "                  and a number to set the size of the large message\n"
            "                  [THIS OPTION MAY CHANGE IN FUTURE RELEASES].\n"
            "-if FLAGSLIST     A comma-delimited list of keywords for operation on\n"
            "                  InfiniBand networks.  The keywords supported are \"ack\",\n"
            "                  \"noack\", \"checksum\", and \"nochecksum\"\n"
            "                  [THIS OPTION MAY CHANGE IN FUTURE RELEASES].\n"
            "\n");
    fflush(stream);
}


static void Help(void)
{
    Usage(stdout);
    exit(EXIT_SUCCESS);
}


static void ListOptions(void)
{
    printf("Advanced options:\n");
    for (int i = 0; i < SizeOfOptions; i++) {
        if (!Options[i].display) {
            printf("    %s", Options[i].AbreviatedName);
            switch (Options[i].TypeOfArgs) {
            case STRING_ARGS:
                printf(" [ARG]...");
                break;
            case NO_ARGS:
            default:
                break;
            }
            printf("\n        %s [%s]",
                   Options[i].Usage, Options[i].FileName);
        }
        printf("\n");
    }
    exit(EXIT_SUCCESS);
}


static void Version(void)
{
    printf("LA-MPI: Version " PACKAGE_VERSION "\n"
           "mpirun built on " __DATE__ " at " __TIME__ "\n");
    exit(EXIT_SUCCESS);
}


void FatalNoInput(const char *ErrorString)
{
    ulm_err(("Error: No input data provided.\n%s\n", ErrorString));
    exit(EXIT_FAILURE);
}


/*
 *  No-op function sed when the corresponding input data is not
 *  requred to run a job, and was not provided at run time.
 */
void NoOpFunction(const char *ErrorString)
{
    return;
}


/*
 *  Debug function for inromational purposes.
 */
void NotImplementedFunction(const char *ErrorString)
{
    ulm_err(("Error: Function not implemented: %s\n",
             ErrorString));
}


static void ScanForHelp(int i, char **argv)
{
    if (strcmp(argv[i], "-h") == 0) {
        Help();
    } else if (strcmp(argv[i], "-help") == 0) {
        Help();
    } else if (strcmp(argv[i], "-list-options") == 0) {
        ListOptions();
    } else if (strcmp(argv[i], "-version") == 0) {
        Version();
    }
}

/* scan a system-wide configuration file, if it exists...
 */
static void ScanSystemConfigFile(void)
{
    int NBytes, OptionIndex;
    char ConfigFileName[ULM_MAX_PATH_LEN],
        InputBuffer[ULM_MAX_CONF_FILELINE_LEN];
    FILE *ConfigFileStream;

    if (getenv("MPI_ROOT") == NULL) {
        return;
    }

    NBytes =
        sprintf(ConfigFileName, "%s/etc/lampi.conf", getenv("MPI_ROOT"));
    if ((NBytes > (ULM_MAX_PATH_LEN - 1)) || (NBytes < 0)) {
        ulm_err(("Error: writing to ConfigFileName (%d)\n",
                 NBytes));
        Abort();
    }

    ConfigFileStream = fopen(ConfigFileName, "r");
    if (ConfigFileStream == NULL) {
        /* no conf file so return */
        return;
    }

    /* position file position indicator to begining of file */
    rewind(ConfigFileStream);

    /* read config file until EOF detected */
    while (fgets
           (InputBuffer, ULM_MAX_CONF_FILELINE_LEN - 1, ConfigFileStream)
           != NULL) {

        /* check to see that first character is not comment # */
        /* if comment - continue */
        if (InputBuffer[0] == '#')
            continue;


        /* parse input line */
        ParseString InputData(InputBuffer, 3, " \n\t");
        if (InputData.GetNSubStrings() == 0)
            continue;

        /* loop over option list and find match */
        OptionIndex = MatchOption(*InputData.begin());
        if (OptionIndex < 0) {
            ulm_err(("Error: Unrecognized option (%s)\n",
                     *InputData.begin()));
            Abort();
        }

        /* fill in InputData */
        if (Options[OptionIndex].TypeOfArgs == NO_ARGS) {
            sprintf(Options[OptionIndex].InputData, "yes");
            Options[OptionIndex].fpToExecute =
                Options[OptionIndex].fpProcessInput;
        } else {
            if (InputData.GetNSubStrings() == 2) {
                ParseString::iterator i = InputData.begin();
                i++;
                sprintf(Options[OptionIndex].InputData, "%s", *i);
                Options[OptionIndex].fpToExecute =
                    Options[OptionIndex].fpProcessInput;
            } else {
                ulm_err(("Error: Cannot parse option %s in configuration file.\n", *InputData.begin()));
                Abort();
            }
        }
    }                           /* end of loop reading ConfigFileStream */
    /* close config file */
    fclose(ConfigFileStream);
}

/*
 * scan the ~/.lampi.conf file and enter this data in Options
 */
static void ScanConfigFile(void)
{
    int NBytes, OptionIndex;
    char ConfigFileName[ULM_MAX_PATH_LEN],
        InputBuffer[ULM_MAX_CONF_FILELINE_LEN];
    struct passwd *LoginStruct;
    FILE *ConfigFileStream;

    LoginStruct = getpwuid(getuid());
    if (LoginStruct == NULL) {
        ulm_err(("Error: finding account information from uid %d\n", getuid()));
        Abort();
    }
    NBytes =
        sprintf(ConfigFileName, "%s/.lampi.conf", LoginStruct->pw_dir);
    if ((NBytes > (ULM_MAX_PATH_LEN - 1)) || (NBytes < 0)) {
        ulm_err(("Error: writing to ConfigFileName (%d)\n",
                 NBytes));
        Abort();
    }

    ConfigFileStream = fopen(ConfigFileName, "r");
    if (ConfigFileStream == NULL) {
        /* no conf file so return */
        return;
    }

    /* position file position indicator to begining of file */
    rewind(ConfigFileStream);

    /* read config file until EOF detected */
    while (fgets
           (InputBuffer, ULM_MAX_CONF_FILELINE_LEN - 1, ConfigFileStream)
           != NULL) {

        /* check to see that first character is not comment # */
        /* if comment - continue */
        if (InputBuffer[0] == '#')
            continue;


        /* parse input line */
        ParseString InputData(InputBuffer, 3, " \n\t");
        if (InputData.GetNSubStrings() == 0)
            continue;

        /* loop over option list and find match */
        OptionIndex = MatchOption(*InputData.begin());
        if (OptionIndex < 0) {
            ulm_err(("Error: Unrecognized option (%s)\n",
                     *InputData.begin()));
            Abort();
        }

        /* fill in InputData */
        if (Options[OptionIndex].TypeOfArgs == NO_ARGS) {
            sprintf(Options[OptionIndex].InputData, "yes");
            Options[OptionIndex].fpToExecute =
                Options[OptionIndex].fpProcessInput;
        } else {
            if (InputData.GetNSubStrings() == 2) {
                ParseString::iterator i = InputData.begin();
                i++;
                sprintf(Options[OptionIndex].InputData, "%s", *i);
                Options[OptionIndex].fpToExecute =
                    Options[OptionIndex].fpProcessInput;
            } else {
                ulm_err(("Error: Cannot parse option %s in configuration file.\n", *InputData.begin()));
                Abort();
            }
        }
    }                           /* end of loop reading ConfigFileStream */
    /* close config file */
    fclose(ConfigFileStream);
}


/*
 * Check the environment for Input parameters.  Any data found is
 * added to the input database.
 */
static void ScanEnvironment(void)
{
    int OptionIndex;
    char *EnvData;

    /* loop over recognized environment variables (same as those
     *   expected in the default database file. */
    for (OptionIndex = 0; OptionIndex < SizeOfOptions; OptionIndex++) {

        /* check for environment variable */
        EnvData = NULL;
        EnvData = getenv(Options[OptionIndex].FileName);

        /* if no match, continue */
        if (EnvData == NULL)
            continue;

        /* Fill in data, if need be */
        if (Options[OptionIndex].TypeOfArgs == NO_ARGS) {
            sprintf(Options[OptionIndex].InputData, "yes");
            Options[OptionIndex].fpToExecute =
                Options[OptionIndex].fpProcessInput;
        } else if (Options[OptionIndex].TypeOfArgs == STRING_ARGS) {
            sprintf(Options[OptionIndex].InputData, "%s", EnvData);
            Options[OptionIndex].fpToExecute =
                Options[OptionIndex].fpProcessInput;
        } else {
            ulm_err(("Error: Cannot parse option %s\n",
                     Options[OptionIndex].FileName));
            Abort();
        }
    }                           /* end OptionIndex loop */
}


/*
 *  This routine is used to scan argv for input options.
 */
static void ScanCmdLine(int argc, char **argv, int *FirstAppArg)
{
    int ArgvIndex, BinaryNameIndex, OptionIndex, Flag;
    size_t ArgvLen, DatabaseLen;
    bool processedBinaryName = false;

    /* set appargsindex to default value indicating no arguments */
    *FirstAppArg = argc;

    BinaryNameIndex =
        MatchOption("BinaryName");
    if (BinaryNameIndex < 0) {
        ulm_err(("Error: Option BinaryName not found\n"));
        Abort();
    }

    /* loop over argv data */
    for (ArgvIndex = 1; ArgvIndex < argc; ArgvIndex++) {
        ArgvLen = strlen(argv[ArgvIndex]);
        Flag = 0;

        ScanForHelp(ArgvIndex, argv);

        /* loop over database options */
        for (OptionIndex = 0; OptionIndex < SizeOfOptions;
             OptionIndex++) {

            /* check AbreviatedName */
            DatabaseLen =
                strlen(Options[OptionIndex].AbreviatedName);
            if ((DatabaseLen == ArgvLen)
                &&
                (strncmp
                 (argv[ArgvIndex],
                  Options[OptionIndex].AbreviatedName,
                  DatabaseLen) == 0)) {
                Flag = 1;
            }
            /* process data if Flag == 1 */
            if (Flag) {
                if (Options[OptionIndex].TypeOfArgs == NO_ARGS) {
                    sprintf(Options[OptionIndex].InputData, "yes");
                    Options[OptionIndex].fpToExecute =
                        Options[OptionIndex].fpProcessInput;
                } else if (Options[OptionIndex].TypeOfArgs ==
                           STRING_ARGS) {
                    if ((ArgvIndex + 1) < argc) {
                        sprintf(Options[OptionIndex].InputData,
                                "%s", argv[++ArgvIndex]);
                        Options[OptionIndex].fpToExecute =
                            Options[OptionIndex].fpProcessInput;
                    }
                    if (OptionIndex == BinaryNameIndex) {
                        processedBinaryName = true;
                    }
                } else {
                    ulm_err(("Error: Cannot parse option %s\n",
                             Options[OptionIndex].FileName));
                    Abort();
                }
                break;
            }

        }                       /* end OptionIndex loop */

        /* stop processing - found the binary, binary arguments, or a bad option */
        if (!Flag) {
            if (processedBinaryName) {
                *FirstAppArg = ArgvIndex;
                break;
            } else {
                if (argv[ArgvIndex][0] == '-') {
                    ulm_err(("Error: unrecognized option \'%s\'.\n", argv[ArgvIndex]));
                    Abort();
                } else {
                    sprintf(Options[BinaryNameIndex].InputData,
                            "%s", argv[ArgvIndex]);
                    Options[BinaryNameIndex].fpToExecute =
                        Options[BinaryNameIndex].fpProcessInput;
                    if (++ArgvIndex < argc) {
                        *FirstAppArg = ArgvIndex;
                    }
                    break;
                }
            }
        }

    }                           /* end ArgvIndex loop */
}


/*
 * Scan the various input sources for input data to the job.  The
 * sources to scan (starting with lowest priority and working one's
 * way up are:
 *      ${MPI_ROOT}/etc/lampi.conf
 *      ~/.lampi.conf
 *      environment variables
 *      input line
 * Function pointers to functions used to process the input are also
 * setup.
 */
void ScanInput(int argc, char **argv, int *FirstAppArg)
{
    /* read in data from ${MPI_ROOT}/etc/lampi.conf */
    ScanSystemConfigFile();

    /* read in data from ~/.lampi.conf */
    ScanConfigFile();

    /* read in data from environment */
    ScanEnvironment();

    /* read in data from command line options */
    ScanCmdLine(argc, argv, FirstAppArg);
}


/*
 * Match the StringToMatch to a variable in the database.  A search is
 * performed over all available types of option names.  The index in
 * the options data base table is returned.
 */
int MatchOption(char *StringToMatch)
{
    int IndexInTable, IndexOfMatch = -1;
    size_t StringToMatchlen, OptionInTablelen;

    /* initialize local data */
    StringToMatchlen = strlen(StringToMatch);

    for (IndexInTable = 0; IndexInTable < SizeOfOptions;
         IndexInTable++) {

        /* search over Abreviated name */
        OptionInTablelen =
            strlen(Options[IndexInTable].AbreviatedName);
        if ((StringToMatchlen == OptionInTablelen)
            &&
            (strncmp
             (Options[IndexInTable].AbreviatedName, StringToMatch,
              StringToMatchlen) == 0)) {
            return IndexInTable;
        }

        /* search over FileName */
        OptionInTablelen = strlen(Options[IndexInTable].FileName);
        if ((StringToMatchlen == OptionInTablelen) &&
            (strncmp(Options[IndexInTable].FileName,
                     StringToMatch, StringToMatchlen) == 0)) {
            return IndexInTable;
        }

    }                           /* end loop over IndexInTable */

    return IndexOfMatch;
}


/*
 * Parse character strings and fill an input array with integer data.
 */
void FillIntData(ParseString * InputObj, int SizeOfArray, int *Array,
                 Options_t * Options, int OptionIndex,
                 int MinVal)
{
    int Value, i, cnt;
    char *TmpCharPtr;

    /* make sure the correct amount of data is present */
    cnt = InputObj->GetNSubStrings();
    if ((cnt != 1) && (cnt != SizeOfArray)) {
        ulm_err(("Error: Wrong number of data elements specified for %s. "
                 "Number or arguments specified (%d) should be either 1 or %d\n",
                 Options[OptionIndex].AbreviatedName,
                 cnt, SizeOfArray));
        Abort();
    }

    /* fill in array */
    if (cnt == 1) {
        /* 1 data element */
        Value = (int) strtol(*(InputObj->begin()), &TmpCharPtr, 10);
        if (TmpCharPtr == *(InputObj->begin())) {
            ulm_err(("Error: Unable to convert data (%s) to integer\n", *(InputObj->begin())));
            Abort();
        }
        if (Value < MinVal) {
            ulm_err(("Error: Unexpected value.  Min value expected %d and value %d\n", MinVal, Value));
            Abort();
        }
        for (i = 0; i < SizeOfArray; i++)
            Array[i] = Value;
    } else {
        /* cnt data elements */
        i = 0;
        for (ParseString::iterator j = InputObj->begin();
             j != InputObj->end(); j++) {
            Value = (int) strtol(*j, &TmpCharPtr, 10);
            if (TmpCharPtr == *j) {
                ulm_err(("Error: Unable to convert data to integer.\n Data :: %s\n", *j));
                Abort();
            }
            if (Value < MinVal) {
                ulm_err(("Error: Unexpected value.  Min value expected %d and value %d\n", MinVal, Value));
                Abort();
            }
            Array[i++] = Value;
        }
    }
}


/*
 * Parse character strings and fill an input array with long data.
 */
void FillLongData(ParseString * InputObj, int SizeOfArray, long *Array,
                  Options_t * Options, int OptionIndex,
                  int MinVal)
{
    int i, cnt;
    char *TmpCharPtr;
    long Value;

    /* make sure the correct amount of data is present */
    cnt = InputObj->GetNSubStrings();
    if ((cnt != 1) && (cnt != SizeOfArray)) {
        ulm_err(("Error: Wrong number of data elements specified for  %s, or %s\n", Options[OptionIndex].AbreviatedName, Options[OptionIndex].FileName));
        ulm_err(("Number or arguments specified is %d , but should be either 1 or %d\n", cnt, SizeOfArray));
        ulm_err(("Input line: %s\n",
                 Options[OptionIndex].InputData));
        Abort();
    }

    /* fill in array */
    if (cnt == 1) {
        /* 1 data element */
        Value = (int) strtol(*(InputObj->begin()), &TmpCharPtr, 10);
        if (TmpCharPtr == *(InputObj->begin())) {
            ulm_err(("Error: Unable to convert data (%s) to integer\n", *(InputObj->begin())));
            Abort();
        }
        if (Value < MinVal) {
            ulm_err(("Error: Unexpected value.  Min value expected %d and value %ld\n", MinVal, Value));
            Abort();
        }
        for (i = 0; i < SizeOfArray; i++)
            Array[i] = Value;
    } else {
        /* cnt data elements */
        i = 0;
        for (ParseString::iterator j = InputObj->begin();
             j != InputObj->end(); j++) {
            Value = (int) strtol(*j, &TmpCharPtr, 10);
            if (TmpCharPtr == *j) {
                ulm_err(("Error: Unable to convert data (%s) to integer\n", *j));
                Abort();
            }
            if (Value < MinVal) {
                ulm_err(("Error: "
                         "Unexpected value.  Min value expected %d and value %ld\n",
                         MinVal, Value));
                Abort();
            }
            Array[i++] = Value;
        }
    }
}


/*
 * Parse character strings and fill an input array with ssize_t data.
 */
void FillSsizeData(ParseString * InputObj, int SizeOfArray,
                   ssize_t * Array, Options_t * Options,
                   int OptionIndex, int MinVal)
{


    /* local data */
    int i, cnt;
    char *TmpCharPtr;
    ssize_t Value;

    /* make sure the correct amount of data is present */
    cnt = InputObj->GetNSubStrings();
    if ((cnt != 1) && (cnt != SizeOfArray)) {
        ulm_err(("Error: "
                 "Wrong number of data elements specified for %s, or %s\n",
                 Options[OptionIndex].AbreviatedName,
                 Options[OptionIndex].FileName));
        ulm_err(("Number or arguments specified is %d , but should be either 1 or %d\n", cnt, SizeOfArray));
        ulm_err(("Input line: %s\n",
                 Options[OptionIndex].InputData));
        Abort();
    }

    /* fill in array */
    if (cnt == 1) {
        /* 1 data element */
        Value = (int) strtol(*(InputObj->begin()), &TmpCharPtr, 10);
        if (TmpCharPtr == *(InputObj->begin())) {
            ulm_err(("Error: Unable to convert data (%s) to integer\n", *(InputObj->begin())));
            Abort();
        }
        if (Value < MinVal) {
            ulm_err(("Error: "
                     "Unexpected value.  Min value expected %d and value %ld\n",
                     MinVal, (long) Value));
            Abort();
        }
        for (i = 0; i < SizeOfArray; i++)
            Array[i] = Value;
    } else {
        /* cnt data elements */
        i = 0;
        for (ParseString::iterator j = InputObj->begin();
             j != InputObj->end(); j++) {
            Value = (int) strtol(*j, &TmpCharPtr, 10);
            if (TmpCharPtr == *j) {
                ulm_err(("Error: Unable to convert data (%s) to integer\n", *j));
                Abort();
            }
            if (Value < MinVal) {
                ulm_err(("Error: "
                         "Unexpected value.  Min value expected %d and value %ld\n",
                         MinVal, (long) Value));
                Abort();
            }
            Array[i++] = Value;
        }
    }
}


/*
 * Fill in directory information.  If a root directory is not
 * specified, $PWD is prepended.
 */
void GetDirectoryList(char *Option, DirName_t ** Array, int ArrayLength)
{
    int NSeparators = 2, cnt, j;
    char SeparatorList[] = { " , " };
    ParseString::iterator i;
    DirName_t DirName;

    /* find ProcsPerHost index in option list */
    int OptionIndex =
        MatchOption(Option);
    if (OptionIndex < 0) {
        ulm_err(("Error: "
                 "Option WorkingDir not found\n"));
        Abort();
    }

    /* parse host list */
    ParseString DirData(Options[OptionIndex].InputData,
                        NSeparators, SeparatorList);

    /* Check to see if expected data is present */
    cnt = DirData.GetNSubStrings();
    if ((cnt != 1) && (cnt != ArrayLength)) {
        ulm_err(("Error: "
                 " Wrong number of data elements specified for  %s, or %s\n",
                 Options[OptionIndex].AbreviatedName,
                 Options[OptionIndex].FileName));
        ulm_err(("Number or arguments specified is :: %d , but should be either 1 or %d\n", cnt, ArrayLength));
        ulm_err(("Input line: %s\n",
                 Options[OptionIndex].InputData));
        Abort();
    }


    /* allocate memory for the array */
    *Array = ulm_new(DirName_t, ArrayLength);

    /* fill in data */

    if (cnt == 1) {
        /* 1 data elements */
        i = DirData.begin();
        if ((*i)[0] != '/') {
            /* root directory not specified */
            if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
                ulm_err(("Error: getcwd() call failed\n"));
                perror(" getcwd ");
                Abort();
            }
            for (int j = 0; j < ArrayLength; j++) {
                sprintf((*Array)[j], "%s/%s", (char *) DirName, (*i));
            }
        } else {
            /* root directory specified */
            for (int j = 0; j < ArrayLength; j++) {
                sprintf((*Array)[j], "%s", (*i));
            }
        }

    } else {
        /* cnt data elements */
        for (j = 0, i = DirData.begin();
             i != DirData.end(), j < ArrayLength; j++, i++) {
            if ((*i)[0] != '/') {
                /* root directory not specified */
                if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
                    ulm_err(("Error: getcwd() call failed\n"));
                    perror(" getcwd ");
                    Abort();
                }
                sprintf((*Array)[j], "%s/%s", (char *) DirName, (*i));
            } else {
                /* root directory specified */
                sprintf((*Array)[j], "%s", (*i));
            }
        }                       /* end loop over entries */
    }                           /* end fill in data */
}



/*
 * This routine is used to process the Application's dirctory name.
 */
void GetAppDir(const char *InfoStream)
{
    GetDirectoryList("DirectoryOfBinary", &(RunParams.UserAppDirList),
                     RunParams.NHosts);
}


/*
 * Determine behaviour when a resource such as descriptors, memory for
 * user data, etc. is exhuaseted.  The default behaviour is for the
 * library to abort, but this can be modified to return an error code
 * to the calling routined.
 */
void parseOutOfResourceContinue(const char *InfoStream)
{
    //
    // allocate memory for the array
    //
    //  SMP message frag descriptors
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        outOfResourceAbort = ulm_new(bool, RunParams.NHosts);
    //  SMP isend descriptors
    RunParams.Networks.SharedMemSetup.isendDescResources_m.
        outOfResourceAbort = ulm_new(bool, RunParams.NHosts);
    //  irecv descriptors
    RunParams.irecvResources_m.outOfResourceAbort =
        ulm_new(bool, RunParams.NHosts);
    //  SMP data buffers
    RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
        outOfResourceAbort = ulm_new(bool, RunParams.NHosts);

    //
    // set outOfResourceAbort indicating to return an error code when out of resources
    //
    for (int host = 0; host < RunParams.NHosts; host++) {
        //  SMP message frag descriptors
        RunParams.Networks.SharedMemSetup.recvFragResources_m.
            outOfResourceAbort[host] = false;
        //  SMP isend descriptors
        RunParams.Networks.SharedMemSetup.isendDescResources_m.
            outOfResourceAbort[host] = false;
        //  irecv descriptors
        RunParams.irecvResources_m.outOfResourceAbort[host] = false;

        //  SMP data buffers
        RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
            outOfResourceAbort[host] = false;

    }

}


/*
 * Fill in the number of consecutive times the library should try to
 * get more resouces before deciding resources are exhausted.
 */
void parseMaxRetries(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MaxRetryWhenNoResource");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MaxRetryWhenNoResource not found\n"));
        Abort();
    }
    //
    // allocate memory for the array
    //
    // SMP message frag headers
    RunParams.Networks.SharedMemSetup.recvFragResources_m.retries_m =
        ulm_new(long, RunParams.NHosts);
    // SMP isend message descriptors
    RunParams.Networks.SharedMemSetup.isendDescResources_m.retries_m =
        ulm_new(long, RunParams.NHosts);
    // irecv descriptors
    RunParams.irecvResources_m.retries_m =
        ulm_new(long, RunParams.NHosts);
    // SMP data buffers
    RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
        retries_m = ulm_new(long, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    // SMP message frag headers
    FillLongData(&ProcData, RunParams.NHosts,
                 RunParams.Networks.SharedMemSetup.recvFragResources_m.
                 retries_m, Options, OptionIndex, 1);

    // SMP isend message descriptors
    for (int host = 0; host < RunParams.NHosts; host++) {
        RunParams.Networks.SharedMemSetup.isendDescResources_m.
            retries_m[host] =
            RunParams.Networks.SharedMemSetup.recvFragResources_m.
            retries_m[host];
        RunParams.irecvResources_m.retries_m[host] =
            RunParams.Networks.SharedMemSetup.recvFragResources_m.
            retries_m[host];
        RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
            retries_m[host] =
            RunParams.Networks.SharedMemSetup.recvFragResources_m.
            retries_m[host];
    }

}


/*
 * Fill in the minimum number of pages per context to use for on host
 * fragement descriptors.
 */
void parseMinSMPRecvDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MinSMPrecvdescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MinSMPrecvdescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        minPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  recvFragResources_m.minPagesPerContext_m,
                  Options, OptionIndex, 1);
}


/*
 * Fill in the maximum number of pages per context to use for on host
 * fragement descriptors.
 */
void parseMaxSMPRecvDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MaxSMPrecvdescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MaxSMPrecvdescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        maxPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  recvFragResources_m.maxPagesPerContext_m,
                  Options, OptionIndex, -1);
}


/*
 * Fill in the maximum number of pages to use for on host fragement
 * descriptors.
 */
void parseTotalSMPRecvDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("TotSMPrecvdescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotSMPrecvdescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        maxTotalPages_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  recvFragResources_m.maxTotalPages_m, Options,
                  OptionIndex, -1);
}


/*
 * Fill in the minimum number of pages per context to use for on host
 * Isend message descriptors.
 */
void parseMinSMPISendDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MinSMPisenddescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MinSMPisenddescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.isendDescResources_m.
        minPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  isendDescResources_m.minPagesPerContext_m,
                  Options, OptionIndex, 1);
}


/*
 * Fill in the maximum number of pages per context to use for on host
 * isend message descriptors
 */
void parseMaxSMPISendDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MaxSMPisenddescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MaxSMPisenddescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.isendDescResources_m.
        maxPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  isendDescResources_m.maxPagesPerContext_m,
                  Options, OptionIndex, -1);
}


/*
 * Fill in the maximum number of pages to use for on host isend
 * message descriptors
 */
void parseTotalSMPISendDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("TotSMPisenddescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotSMPisenddescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.isendDescResources_m.
        maxTotalPages_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  isendDescResources_m.maxTotalPages_m, Options,
                  OptionIndex, -1);
}


/*
 * Fill in the maximum number of pages to use for irecv descriptors
 */
void parseTotalIRecvDescPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("TotIrecvdescpages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotIrecvdescpages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.irecvResources_m.maxTotalPages_m =
        ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.irecvResources_m.maxTotalPages_m,
                  Options, OptionIndex, -1);
}


/*
 * Fill in the minimum number of pages per context to use for SMP data
 * buffers
 */
void parseMinSMPDataPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MinSMPDatapages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MinSMPDatapages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
        minPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  SMPDataBuffersResources_m.minPagesPerContext_m,
                  Options, OptionIndex, 1);
}


/*
 * Fill in the maximum number of pages per context to use for SMP data
 * buffers
 */
void parseMaxSMPDataPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MaxSMPDatapages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MaxSMPDatapages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
        maxPagesPerContext_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  SMPDataBuffersResources_m.maxPagesPerContext_m,
                  Options, OptionIndex, -1);
}


/*
 * Fill in the maximum number of pages to use for SMP data buffers
 */
void parseTotalSMPDataPages(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("TotSMPDatapages");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotSMPDatapages not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.Networks.SharedMemSetup.SMPDataBuffersResources_m.
        maxTotalPages_m = ulm_new(ssize_t, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillSsizeData(&ProcData, RunParams.NHosts,
                  RunParams.Networks.SharedMemSetup.
                  SMPDataBuffersResources_m.maxTotalPages_m,
                  Options, OptionIndex, -1);
}


void setNoLSF(const char *InfoStream)
{
    RunParams.UseLSF = false;
}

/*
 * Set a flag indicating threads will not be used in this run.
 */
void setThreads(const char *InfoStream)
{
    RunParams.UseThreads = 1;
}


void setUseSSH(const char *InfoStream)
{
    RunParams.UseSSH = 1;
}


void setLocal(const char *InfoStream)
{
    RunParams.Local = 1;
}


void SetOutputPrefixTrue(const char *InfoStream)
{
    RunParams.OutputPrefix = 1;
}


void SetQuietTrue(const char *InfoStream)
{
    RunParams.Quiet = 1;
    RunParams.Verbose = 0;
}


void SetVerboseTrue(const char *InfoStream)
{
    RunParams.Quiet = 0;
    RunParams.Verbose = 1;
    RunParams.OutputPrefix = 1;
}


void SetCheckArgsFalse(const char *InfoStream)
{
    RunParams.CheckArgs = 0;

    putenv("LAMPI_NO_CHECK_ARGS=1");
}


#if ENABLE_NUMA

/*
 * Fill in number of cpus per node to use
 */
void parseCpusPerNode(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex =
        MatchOption("CpusPerNode");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option CpusPerNode not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.nCpusPerNode = ulm_new(int, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillIntData(&ProcData, RunParams.NHosts,
                RunParams.nCpusPerNode, Options, OptionIndex,
                1);
}


/*
 * Specify if resrouce affinity will be used
 */
void parseResourceAffinity(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("ResourceAffinity");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option ResourceAffinity not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.useResourceAffinity = ulm_new(int, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillIntData(&ProcData, RunParams.NHosts,
                RunParams.useResourceAffinity, Options,
                OptionIndex, 0);
}


/*
 * Specify how resource affinity will be specified
 */
void parseDefaultAffinity(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("DefaultAffinity");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option DefaultAffinity not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.defaultAffinity = ulm_new(int, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillIntData(&ProcData, RunParams.NHosts,
                RunParams.defaultAffinity, Options,
                OptionIndex, 0);
}


/*
 * Specify what to do if resource affinity request can't be satisfied
 */
void parseMandatoryAffinity(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex = MatchOption("MandatoryAffinity");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MandatoryAffinity not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.mandatoryAffinity = ulm_new(int, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillIntData(&ProcData, RunParams.NHosts,
                RunParams.mandatoryAffinity, Options,
                OptionIndex, 0);
}

#endif                          // ENABLE_NUMA


/*
 * Specify the maximum number of communicators per host that the
 * library can support
 */
void parseMaxComms(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    /* find ProcsPerHost index in option list */
    int OptionIndex =
        MatchOption("MaxComms");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MaxComms not found\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.maxCommunicators = ulm_new(int, RunParams.NHosts);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillIntData(&ProcData, RunParams.NHosts,
                RunParams.maxCommunicators, Options,
                OptionIndex, 0);
}


/*
 * Specify the number of bytes of memory per process to allocate for
 * the Shared Memory descriptor pool
 */
void parseSMDescMemPerProc(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    fillInputStructure(&(RunParams.smDescPoolBytesPerProc),
                       "SMDescMemPerProc", RunParams.NHosts,
                       (ssize_t) 1, NSeparators, SeparatorList);
}

/*
 * specify which Quadrics rails to use by rail index starting from 0
 */
void parseQuadricsRails(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    int OptionIndex = MatchOption("QuadricsRails");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option QuadricsRails not found\n"));
        Abort();
    }

    ParseString HostData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    RunParams.quadricsRailMask = 0;

    for (ParseString::iterator i = HostData.begin(); i != HostData.end();
         i++) {
        int railIndex = atoi(*i);
        if ((railIndex < 0) || (railIndex > 31)) {
            ulm_err(("Error: specified Quadrics rail index, %d, is outside " "acceptable limits (0 >= n < 32)\n", railIndex));
            Abort();
        }
        RunParams.quadricsRailMask |= (1 << railIndex);
    }
}

void parseUseCRC(const char *InfoStream)
{
    RunParams.UseCRC = true;
}

void parseQuadricsFlags(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    int OptionIndex =
        MatchOption("QuadricsFlags");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option QuadricsFlags not found\n"));
        Abort();
    }

    ParseString QuadricsFlags(Options[OptionIndex].InputData,
                              NSeparators, SeparatorList);

    for (ParseString::iterator i = QuadricsFlags.begin();
         i != QuadricsFlags.end(); i++) {
        if ((strlen(*i) == 5) && (strncmp(*i, "noack", 5) == 0)) {
            RunParams.quadricsDoAck = false;
        } else if ((strlen(*i) == 10)
                   && (strncmp(*i, "nochecksum", 10) == 0)) {
            RunParams.quadricsDoChecksum = false;
        } else if ((strlen(*i) == 4) && (strncmp(*i, "nohw", 4) == 0)) {
            RunParams.quadricsHW = 0;
        } else if ((strlen(*i) == 2) && (strncmp(*i, "hw", 2) == 0)) {
            RunParams.quadricsHW = 1;
        } else if ((strlen(*i) == 3) && (strncmp(*i, "ack", 3) == 0)) {
            RunParams.quadricsDoAck = true;
        } else if ((strlen(*i) == 8) && (strncmp(*i, "checksum", 8) == 0)) {
            RunParams.quadricsDoChecksum = true;
        } else {
            ulm_err(("Error: unrecognized -qf argument: %s \n",*i));
            Abort();
        }
    }
}

void parseMyrinetFlags(const char *InfoStream)
{
#if ENABLE_GM
    int NSeparators = 2;
    char SeparatorList[] = { " , " };
    char *endptr;
    long fragSize;

    int OptionIndex =
        MatchOption("MyrinetFlags");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option MyrinetFlags not found\n"));
        Abort();
    }

    ParseString QuadricsFlags(Options[OptionIndex].InputData,
                              NSeparators, SeparatorList);

    for (ParseString::iterator i = QuadricsFlags.begin();
         i != QuadricsFlags.end(); i++) {
        if ((strlen(*i) >= 3) && (strncmp(*i, "noack", 3) == 0)) {
            RunParams.Networks.GMSetup.doAck = false;
        } else if ((strlen(*i) >= 3)
                   && (strncmp(*i, "nochecksum", 3) == 0)) {
            RunParams.Networks.GMSetup.doChecksum = false;
        } else if ((strlen(*i) >= 1) && (strncmp(*i, "ack", 1) == 0)) {
            RunParams.Networks.GMSetup.doAck = true;
        } else if ((strlen(*i) >= 1) && (strncmp(*i, "checksum", 1) == 0)) {
            RunParams.Networks.GMSetup.doChecksum = true;
        } else if ((strlen(*i) >= 1)
                   && (fragSize = strtol(*i, &endptr, 10))
                   && (*endptr == '\0')) {
            RunParams.Networks.GMSetup.fragSize = fragSize;
        }
    }
#endif
}

void parseIBFlags(const char *InfoStream)
{
#if ENABLE_INFINIBAND
    int NSeparators = 2;
    char SeparatorList[] = { " , " };

    int OptionIndex =
        MatchOption("IBFlags");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option IBFlags not found\n"));
        Abort();
    }

    ParseString IBFlags(Options[OptionIndex].InputData,
                              NSeparators, SeparatorList);

    for (ParseString::iterator i = IBFlags.begin();
         i != IBFlags.end(); i++) {
        if ((strlen(*i) >= 3) && (strncmp(*i, "noack", 3) == 0)) {
            RunParams.Networks.IBSetup.ack = false;
        } else if ((strlen(*i) >= 3)
                   && (strncmp(*i, "nochecksum", 3) == 0)) {
            RunParams.Networks.IBSetup.checksum = false;
        } else if ((strlen(*i) >= 1) && (strncmp(*i, "ack", 1) == 0)) {
            RunParams.Networks.IBSetup.ack = true;
        } else if ((strlen(*i) >= 1) && (strncmp(*i, "checksum", 1) == 0)) {
            RunParams.Networks.IBSetup.checksum = true;
        }
    }
#endif
}


#if ENABLE_TCP

void parseTCPMaxFragment(const char *InfoStream)
{
    int NSeparators = 1;
    char SeparatorList[] = { " " };

    int OptionIndex =
        MatchOption("TCPMaxFragment");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TCPMaxFragment not found\n"));
        Abort();
    }

    ParseString params(Options[OptionIndex].InputData,
                       NSeparators, SeparatorList);

    for (ParseString::iterator i = params.begin(); i != params.end(); i++) {
        RunParams.Networks.TCPSetup.MaxFragmentSize = atol(*i);
        if(RunParams.Networks.TCPSetup.MaxFragmentSize == 0) {
            ulm_err(("Error: invalid value for option -tcpmaxfrag \"%s\"\n", *i));
            Abort();
        }
    }
}

void parseTCPEagerSend(const char *InfoStream)
{
    int NSeparators = 1;
    char SeparatorList[] = { " " };

    int OptionIndex =
        MatchOption("TCPEagerSend");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TCPEagerSend not found\n"));
        Abort();
    }

    ParseString params(Options[OptionIndex].InputData,
                       NSeparators, SeparatorList);

    for (ParseString::iterator i = params.begin(); i != params.end(); i++) {
        RunParams.Networks.TCPSetup.MaxEagerSendSize = atol(*i);
        if(RunParams.Networks.TCPSetup.MaxEagerSendSize == 0) {
            ulm_err(("Error: invalid value for option -tcpeagersend \"%s\"\n", *i));
            Abort();
        }
    }
}

void parseTCPConnectRetries(const char *InfoStream)
{
    int NSeparators = 1;
    char SeparatorList[] = { " " };

    int OptionIndex =
        MatchOption("TCPConnectRetries");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TCPConnectRetries not found\n"));
        Abort();
    }

    ParseString params(Options[OptionIndex].InputData,
                       NSeparators, SeparatorList);

    for (ParseString::iterator i = params.begin(); i != params.end(); i++) {
        RunParams.Networks.TCPSetup.MaxConnectRetries = atol(*i);
        if(RunParams.Networks.TCPSetup.MaxConnectRetries == 0) {
            ulm_err(("Error: invalid value for option -tcpmaxcon \"%s\"\n", *i));
            Abort();
        }
    }
}

#endif


