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

#ifndef _INPUT
#define _INPUT

#include <stdio.h>
#include <stdlib.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/types.h"
#include "util/ParseString.h"

struct Options_t {
    /* NULL-terminated array of aliases for the command line option
     * name (not including leading dashes) */
    char *OptName[8];
    /* name used in Database file and evnironment variable */
    char *FileName;
    /* enum used to indicate what type of input will accompany this */
    int TypeOfArgs;
    /* this function will be invoked after all input is done */
    void (*fpToExecute) (const char *msg);
    /* if input is done, this function is pointed to by fpToExecute */
    void (*fpProcessInput) (const char *msg);
    /* Tell what the real usage is */
    char *Usage;
    /* display flag */
    int display;
    /* Input data is kept here - fpToExecute will parse this data with
     *  the routine fpProcessInput (invoked by fpToExecute) */
    char InputData[ULM_MAX_CONF_FILELINE_LEN];
};

/*
 * type of input parameters
 * NO_ARGS     - nothing beyond the flag
 * STRING_ARGS - a single string following the flag
 */
enum { NO_ARGS, STRING_ARGS };


/* function prototypes */
int MatchOption(char *StringToMatch);
void FillIntData(ParseString *InputObj, int SizeOfArray, int *Array,
                 Options_t *Options, int OptionIndex,
                 int MinVal);
void GetDirectoryList(char *Option, DirName_t ** Array, int ArrayLength);
void FatalNoInput(const char *msg);
void NoOpFunction(const char *msg);
void NotImplementedFunction(const char *msg);
void SetCheckArgsFalse(const char *msg);
void SetConnectTimeout(const char *msg);
void SetHeartbeatPeriod(const char *msg);
void SetOutputPrefixTrue(const char *msg);
void SetQuietTrue(const char *msg);
void SetVerboseTrue(const char *msg);
void SetWarnTrue(const char *msg);
void SetPrintRusageTrue(const char *msg);
void GetAppDir(const char *Msg);
void GetAppHostCount(const char *msg);
void GetAppHostData(const char *msg);
void GetAppHostDataFromMachineFile(const char *msg);
void GetAppHostDataNoInputRSH(const char *msg);
void GetClientApp(const char *msg);
void GetClientCpus(const char *msg);
void GetClientProcessCount(const char *msg);
void GetClientProcessCountNoInput(const char *msg);
void GetClientWorkingDirectory(const char *msg);
void GetClientWorkingDirectoryNoInp(const char *msg);
void GetGDB(const char *msg);
void GetInterfaceCount(const char *msg);
void GetInterfaceList(const char *msg);
void GetInterfaceNoInput(const char *msg);
void GetLSFResource();
void GetMpirunHostname(const char *msg);
void GetMpirunHostnameNoInput(const char *msg);
void GetNetworkDevList(const char *msg);
void GetNetworkDevListNoInput(const char *msg);
void GetDebugDefault(const char *msg);
void GetDebugDaemon(const char *msg);
void parseCpusPerNode(const char *msg);
void parseDefaultAffinity(const char *msg);
void parseEnvironmentSettings(const char *msg);
void parseIBFlags(const char *msg);
void parseMandatoryAffinity(const char *msg);
void parseMaxComms(const char *msg);
void parseMaxRetries(const char *msg);
void parseMaxSMPDataPages(const char *msg);
void parseMaxSMPISendDescPages(const char *msg);
void parseMaxSMPRecvDescPages(const char *msg);
void parseMaxSMPStrdISendDescPages(const char *msg);
void parseMaxStrdIRecvDescPages(const char *msg);
void parseMinSMPDataPages(const char *msg);
void parseMinSMPISendDescPages(const char *msg);
void parseMinSMPRecvDescPages(const char *msg);
void parseMinSMPStrdISendDescPages(const char *msg);
void parseMinStrdIRecvDescPages(const char *msg);
void parseMyrinetFlags(const char *msg);
void parseOutOfResourceContinue(const char *msg);
void parseQuadricsFlags(const char *msg);
void parseQuadricsRails(const char *msg);
void parseResourceAffinity(const char *msg);
void parseSMDescMemPerProc(const char *msg);
void parseTotalIRecvDescPages(const char *msg);
void parseTotalSMPDataPages(const char *msg);
void parseTotalSMPISendDescPages(const char *msg);
void parseTotalSMPRecvDescPages(const char *msg);
void parseUseCRC(const char *msg);
void setLocal(const char *msg);
void setNoLSF(const char *msg);
void setThreads(const char *msg);
void setUseSSH(const char *msg);
#if ENABLE_TCP
void parseTCPMaxFragment(const char* msg);
void parseTCPEagerSend(const char* msg);
void parseTCPConnectRetries(const char *msg);
#endif

/*
 * global option table and size
 */

extern Options_t Options[];   // command line options
extern int SizeOfOptions;     // size of options array


void AbortFunction(const char *, int);

// class to fill in array of type T
template <class T>
void FillData(ParseString *InputObj, int SizeOfArray, T *Array,
              Options_t *Options, int OptionIndex, T MinVal)
{
    int Value, i, cnt;
    char *TmpCharPtr;

    /* make sure the correct amount of data is present */
    cnt = InputObj->GetNSubStrings();
    if ((cnt != 1) && (cnt != SizeOfArray)) {
        ulm_err(("Error: Wrong number of data elements specified for  %s, or %s\n"
                 "\tNumber or arguments specified is %d, "
                 "but should be either 1 or %d\n"
                 "\tInput line: %s\n",
                 Options[OptionIndex].OptName[0],
                 Options[OptionIndex].FileName,
                 cnt, SizeOfArray,
                 Options[OptionIndex].InputData));
        AbortFunction(__FILE__, __LINE__);
    }

    /* fill in array */
    if (cnt == 1) {
        /* 1 data element */
        Value = (T) strtol(*(InputObj->begin()), &TmpCharPtr, 10);
        if (TmpCharPtr == *(InputObj->begin())) {
            ulm_err(("Error: Can't convert data (%s) to integer\n",
                     *(InputObj->begin())));
            AbortFunction(__FILE__, __LINE__);
        }
        if (Value < MinVal) {
            ulm_err(("Error: Unexpected value. Min value expected %ld and value %d\n",
                     (long) MinVal, Value));
            AbortFunction(__FILE__, __LINE__);
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
                AbortFunction(__FILE__, __LINE__);
            }
            if (Value < MinVal) {
                ulm_err(("Error: Unexpected value. Min value expected %ld and value %d\n",
                         (long) MinVal, Value));
                AbortFunction(__FILE__, __LINE__);
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
    int OptionIndex = MatchOption(inputString);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option %s not found\n", inputString));
        AbortFunction(__FILE__, __LINE__);
    }

    /* allocate memory for the array */
    *dataStorage = ulm_new(T, nElements);

    /* parse ProcsPerHost data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    /* fill in data */
    FillData(&ProcData, nElements, *dataStorage, Options,
             OptionIndex, minValue);
}

#endif /* _INPUT */
