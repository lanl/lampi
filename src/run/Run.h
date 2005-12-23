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

#ifndef _RUN_RUN
#define _RUN_RUN

#include <sys/types.h>
#include <stdio.h>

extern int AbnormalExitStatus;

/* prototypes */

#define Abort() AbortFunction(__FILE__, __LINE__)
void AbortFunction(const char *, int);
int CheckForControlMsgs(void);
void DebuggerInit(void);
void FixRunParams(int nhosts);
void InitProc(void);
int InstallSigHandler(void);
int IsTVDebug(void);
void KillAppProcs(int hostrank);
void LogJobAbort(void);
void LogJobExit(void);
void LogJobMsg(const char *);
void LogJobStart(void);
void PrintRusage(const char *label, struct rusage *ru);
void PrintTotalRusage(void);
int ProcessInput(int argc, char **argv, int *FirstAppArg);
void RearrangeHostList(const char *InfoStream);
void RunEventLoop(void);
void ScanInput(int argc, char **argv, int *FirstAppArg);
int SendInitialInputDataToClients(void);
int Spawn(unsigned int *AuthData, int ReceivingSocket,
          int **ListHostsStarted, int argc, char **argv);
int SpawnBproc(unsigned int *AuthData, int ReceivingSocket,
               int **ListHostsStarted, int argc, char **argv);
int SpawnExec(unsigned int *AuthData, int ReceivingSocket,
              int **ListHostsStarted, int argc, char **argv);
int SpawnLsf(unsigned int *AuthData, int ReceivingSocket,
             int **ListHostsStarted, int argc, char **argv);
int SpawnRms(unsigned int *AuthData, int port,
             int argc, char **argv);
int SpawnRsh(unsigned int *AuthData, int ReceivingSocket,
             int **ListHostsStarted, int argc, char **argv);
void UpdateTotalRusage(struct rusage *ru);
void Usage(FILE *stream);
void VerifyLsfResources(void);

#endif /* _RUN_RUN  */
