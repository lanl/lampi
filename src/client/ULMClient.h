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

#ifndef _ULMCLIENT
#define _ULMCLIENT

#include <sys/types.h>
#include "internal/linkage.h"
#include "internal/types.h"
#include "init/init.h" /* for lampiState_t */

int ClientInstallSigHandler(void);
void Clientsig_child(int Signal);
void ClientAbnormalChildTermination(pid_t PIDofChild, int NChildren,
                                    pid_t *ChildPIDs, int *IAmAlive,
                                    int SocketToULMRun);
void daemonAbnormalChildTermination(pid_t PIDofChild, int NChildren,
                                    pid_t *ChildPIDs, int *IAmAlive,
                                    lampiState_t *s);
void AbortLocalHost(int ServerSocketFD, int *ProcessCount, int _ulm_HostID,
                    pid_t *ChildPIDs, unsigned int MessageType,
                    int Notify);
void AbortAndDrainLocalHost(int ServerSocketFD, int *ProcessCount, int _ulm_HostID,
                    pid_t *ChildPIDs, unsigned int MessageType,
                    int Notify, int *ClientStdoutFDs, int *ClientStderrFDs,
                    int ToServerStdoutFD, int ToServerStderrFD, PrefixName_t *IOPrefix,
                    int *LenIOPreFix, size_t *StderrBytesWritten, size_t *StdoutBytesWritten,
                    int *NewLineLast);
int CheckIfChildrenAlive(int *ProcessCount, int _ulm_HostID,
                         int *IAmAlive);
int checkForRunControlMsgs(int *ServerSocketFD, double *HeartBeatTime, int *ProcessCount,
                              int hostIndex, pid_t *ChildPIDs,
                              int *STDOUTfdsFromChildren,
                              int *STDERRfdsFromChildren, int StdoutFD,
                              int StderrFD, size_t *StderrBytesWritten,
                              size_t *StdoutBytesWritten,
                              int *NewLineLast, PrefixName_t *IOPreFix,
                              int *LenIOPreFix, lampiState_t *state);
int ClientCheckForControlMsgs(int MaxDescriptor, int *ServerSocketFD,
                              double *HeartBeatTime, int *ProcessCount,
                              int _ulm_HostID, pid_t *ChildPIDs,
                              int *STDOUTfdsFromChildren,
                              int *STDERRfdsFromChildren, int StdoutFD,
                              int StderrFD, size_t *StderrBytesWritten,
                              size_t *StdoutBytesWritten,
                              int *NewLineLast, PrefixName_t *IOPreFix,
                              int *LenIOPreFix);
int ClientScanStdoutStderr(int *ClientStdoutFDs, int *ClientStderrFDs,
                           int ToServerStdoutFD, int ToServerStderrFD,
                           int NFDs, int MaxDescriptor,
                           PrefixName_t *IOPreFix, int *LenIOPreFix,
                           size_t *StderrBytesWritten,
                           size_t *StdoutBytesWritten, int *NewLineLast);
void ClientOrderlyShutdown(size_t *StderrBytesWritten,
                           size_t *StdoutBytesWritten,
                           int ControlSocketToULMRunFD,
						   lampiState_t *state);
void ClientDrainSTDIO(int *ClientStdoutFDs, int *ClientStderrFDs,
                      int ToServerStdoutFD, int ToServerStderrFD, int NFDs,
                      int MaxDescriptor, PrefixName_t *IOPreFix,
                      int *LenIOPreFix, size_t *StderrBytesWritten,
                      size_t *StdoutBytesWritten, int *NewLineLast,
                      int ControlSocketToULMRunFD);
void ClientWaitForSetUpToComplete(volatile int *HostDoneWithSetup,
                                  volatile int *Wait, int NHosts,
                                  int IAmDaemon, int ServerSocketFD);
void setupMemoryPools();
int setupCore();
int setupPerProcSharedMemPools(int nPools);

#endif                          /* _ULMCLIENT */
