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



#ifndef _RUN_RUN
#define _RUN_RUN

#include <sys/types.h>

#include "util/ParseString.h"
#include "internal/new.h"
#include "internal/new.h"
#include "run/JobParams.h"
#include "run/Input.h"
#include "client/adminMessage.h"

#ifndef ULM_GLOBAL_DEFINE
#define EXTERN extern
#else
#define EXTERN
#endif

/* prototypes */

int FindStartedClients(int *ListHostAppSpawned);
int IsTVDebug(void);
int SendInitialInputDataToClients(ULMRunParams_t *RunParameters,
		adminMessage *server);
int SendSharedMemInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server);
int SendQuadricsInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server);
int SendGMInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server);
int SpawnUserApp(unsigned int *AuthData, int ReceivingSocket,
                 int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		 int FirstAppArgument, int argc, char **argv);
int SpawnUserAppLSF(unsigned int *AuthData, int ReceivingSocket,
                    int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		    int FirstAppArgument, int argc, char **argv);
int SpawnUserAppRSH(unsigned int *AuthData, int ReceivingSocket,
                    int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		    int FirstAppArgument, int argc, char **argv);
int MPIrunProcessInput(int argc, char **argv, int *NULMArgs,
                       int **IndexULMArgs, ULMRunParams_t *RunParameters,
                       int *FirstAppArgument);
int MPIrunInstallSigHandler(void);
int _ulm_RearrangeHostList(HostName_t *HostList, int *ProcessCnt,
                           DirName_t *WorkingDirList, ExeName_t *ExeList,
                           int NHosts, int NULMArgs, int *IndexULMArgs,
                           DirName_t *ClientDirList,
                           int *NPathTypes,
                           int **ListPathTypes, int argc,
                           char **argv);
int mpirunAbortAllHosts(int *ClientSocketFDList, int NHosts, adminMessage *server);
int mpirunCheckForDaemonMsgs(int NHosts, double *HeartBeatTime,
                              int *HostsNormalTerminated,
                              ssize_t *StderrBytesRead,
                              ssize_t *StdoutBytesRead, int *STDERRfds,
                              int *STDOUTfds, int *HostsAbNormalTerminated,
                              int *ActiveHosts, int *ProcessCnt,
                              pid_t ** PIDsOfAppProcs,
                              int *ActiveClients,
							  adminMessage *server);
int mpirunCheckForControlMsgs(int MaxDescriptor,
                              int *ClientSocketFDList, int NHosts,
                              double *HeartBeatTime,
                              int *HostsNormalTerminated,
                              ssize_t *StderrBytesRead,
                              ssize_t *StdoutBytesRead, int *STDERRfds,
                              int *STDOUTfds,
                              int *HostsAbNormalTerminated,
                              int *ActiveHosts, int *ProcessCnt,
                              pid_t ** PIDsOfAppProcs,
                              double *TimeFirstCheckin,
                              int *ActiveClients);
int mpirunCheckHeartBeat(double *HeartBeatTime, double Time, int NHosts,
                         int *ActiveHosts, int HeartBeatTimeOut);
int mpirunGetTerminateInitiated();
int mpirunScanStdErrAndOut(int *STDERRfds, int *STDOUTfds, int NHosts,
                           int MaxDescriptor, ssize_t *StderrBytesRead,
                           ssize_t *StdoutBytesRead);
int mpirun_spawn_bproc(unsigned int *AuthData, int ReceivingSocket,
                       int **ListHostsStarted, ULMRunParams_t *RunParameters,
                       int FirstAppArgument, int argc, char **argv);
int mpirun_spawn_exec(unsigned int *AuthData, int ReceivingSocket,
                      int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		      int FirstAppArgument, int argc, char **argv);
int mpirun_spawn_prun(unsigned int *AuthData, int port, 
		ULMRunParams_t *RunParameters, int FirstAppArgument, 
		int argc, char **argv);
int mpirun_spawn_rsh(unsigned int *AuthData, char *sock_ip,
                     int **ListHostsStarted, ULMRunParams_t *RunParameters, 
		     int FirstAppArgument, int argc, char **argv);
int parse_cmdln_bproc(int, char **, ULMRunParams_t *);
ssize_t mpirunRecvAndWriteData(int ReadFD, FILE * WriteFile);
void lampirun_init_proc();
void AbortFunction(const char *, int);
#define Abort() AbortFunction(__FILE__, __LINE__)
void GetAppDir(const char *InfoStream);
void GetAppHostCount(const char *InfoStream);
void GetMpirunHostnameNoInput(const char *InfoStream);
void GetMpirunHostname(const char *InfoStream);
void GetAppHostData(const char *InfoStream);
void GetAppHostDataNoInputRSH(const char *InfoStream);
void GetClientApp(const char *InfoStream);
void GetClientCpus(const char *InfoStream);
void GetClientProcessCount(const char *InfoStream);
void GetClientProcessCountNoInput(const char *InfoStream);
void GetClientWorkingDirectory(const char *InfoStream);
void GetClientWorkingDirectoryNoInp(const char *InfoStream);
void GetLSFResource();
void GetNetworkDevListNoInput(const char *InfoStream);
void GetNetworkDevList(const char *InfoStream);
void GetTVAll(const char *InfoStream);
void GetTVDaemon(const char *InfoStream);
void ScanInput(int argc, char **argv);
void TerminateClients(int NClientsAccepted, int *ListClientsAccepted,
                      ULMRunParams_t RunParameters);
void MPIrunDaemonize(ssize_t *StderrBytesRead, ssize_t *StdoutBytesRead,
                     ULMRunParams_t *RunParameters);
void VerifyLsfResources(const ULMRunParams_t *RunParameters);
void _ulm_RearrangeHostList(const char *InfoStream);
void fix_RunParameters(ULMRunParams_t *RunParameters, int nhosts);
void mpirunDrainStdioData(int *STDERRfds, int *STDOUTfds,
                          ssize_t *StderrBytesRead,
                          ssize_t *StdoutBytesRead,
                          ssize_t ExpectedStderrBytesRead,
                          ssize_t ExpectedStdoutBytesRead);
void mpirunKillAppProcs(HostName_t Host, int NProcs, pid_t *AppPIDs);
void mpirunSetTerminateInitiated(int a);
void mpirunsigalarm(int Signal);
void parseCpusPerNode(const char *InfoStream);
void parseDefaultAffinity(const char *InfoStream);
void parseEnvironmentSettings(const char *InfoStream);
void parseMandatoryAffinity(const char *InfoStream);
void parseMaxComms(const char *InfoStream);
void parseMaxRetries(const char *InfoStream);
void parseMaxSMPDataPages(const char *InfoStream);
void parseMaxSMPISendDescPages(const char *InfoStream);
void parseMaxSMPRecvDescPages(const char *InfoStream);
void parseMaxSMPStrdISendDescPages(const char *InfoStream);
void parseMaxStrdIRecvDescPages(const char *InfoStream);
void parseMinSMPDataPages(const char *InfoStream);
void parseMinSMPISendDescPages(const char *InfoStream);
void parseMinSMPRecvDescPages(const char *InfoStream);
void parseMinSMPStrdISendDescPages(const char *InfoStream);
void parseMinStrdIRecvDescPages(const char *InfoStream);
void parseOutOfResourceContinue(const char *InfoStream);
void parseQuadricsRails(const char *InfoStream);
void parseUseCRC(const char *InfoStream);
void parseQuadricsFlags(const char *InfoStream);
void parseMyrinetFlags(const char *InfoStream);
void parseResourceAffinity(const char *InfoStream);
void parseSMDescMemPerProc(const char *InfoStream);
void parseTotalIRecvDescPages(const char *InfoStream);
void parseTotalSMPDataPages(const char *InfoStream);
void parseTotalSMPISendDescPages(const char *InfoStream);
void parseTotalSMPRecvDescPages(const char *InfoStream);
void setNoLSF(const char *InfoStream);
void setThreads(const char *InfoStream);

// class to fill in array of type T
template <class T>
void FillData(ParseString *InputObj, int SizeOfArray, T *Array,
              InputParameters_t *ULMInputOptions, int OptionIndex, T MinVal)
{
    int Value, i, cnt;
    char *TmpCharPtr;

    /* make sure the correct amount of data is present */
    cnt = InputObj->GetNSubStrings();
    if ((cnt != 1) && (cnt != SizeOfArray)) {
        ulm_err(("Error: Wrong number of data elements specified for  %s, or %s\n",
                 "\tNumber or arguments specified is %d, "
                 "but should be either 1 or %d\n"
                 "\tInput line: %s\n",
                 ULMInputOptions[OptionIndex].AbreviatedName,
                 ULMInputOptions[OptionIndex].FileName,
                 cnt, SizeOfArray,
                 ULMInputOptions[OptionIndex].InputData));
        Abort();
    }

    /* fill in array */
    if (cnt == 1) {
        /* 1 data element */
        Value = (T) strtol(*(InputObj->begin()), &TmpCharPtr, 10);
        if (TmpCharPtr == *(InputObj->begin())) {
            ulm_err(("Error: Can't convert data (%s) to integer\n",
                     *(InputObj->begin())));
            Abort();
        }
        if (Value < MinVal) {
            ulm_err(("Error: Unexpected value. Min value expected %ld and value %d\n",
                     (long) MinVal, Value));
            Abort();
        }
        for (i = 0; i < SizeOfArray; i++)
            Array[i] = Value;
    } else {
        /* cnt data elements */
        i = 0;
        for (ParseString::iterator j = InputObj->begin();
             j != InputObj->end(); j++) {
            Value = (T) strtol(*j, &TmpCharPtr, 10);
            if (TmpCharPtr == *j) {
                ulm_err(("Error: Can't convert data (%s) to integer\n", *j));
                Abort();
            }
            if (Value < MinVal) {
                ulm_err(("Error: Unexpected value. Min value expected %ld and value %d\n",
                         (long) MinVal, Value));
                Abort();
            }
            Array[i++] = Value;
        }
    }
}


//! template function for parsing user data
template <class T>
void fillInputStructure(T **dataStorage, char *inputString,
                        int nElements, T minValue, int NSeparators,
                        char *SeparatorList)
{
    // find ProcsPerHost index in option list
    int OptionIndex =
        MatchOption(inputString, ULMInputOptions, SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option %s not found\n", inputString));
        Abort();
    }

    /* allocate memory for the array */
    *dataStorage = ulm_new(T, nElements);

    /* parse ProcsPerHost data */
    ParseString ProcData(ULMInputOptions[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillData(&ProcData, nElements, *dataStorage, ULMInputOptions,
             OptionIndex, minValue);
}

#endif /* _RUN_RUN  */
