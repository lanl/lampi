/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/



#ifndef _INPUT2
#define _INPUT2

#include <stdio.h>
#include <stdlib.h>
#include "internal/constants.h"
#include "Run.h"
#include "Input.h"
#include "util/ParseString.h"

InputParameters_t ULMInputOptions[] =
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
     _ulm_RearrangeHostList,
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
#ifdef ENABLE_NUMA
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
#if defined(ENABLE_TCP)
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

#endif
