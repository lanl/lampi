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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "util/Utility.h"

/*
 * This routine sets up the host information.
 * Input:  NHostInfoFound - found host information
 *         arc, argv
 *         NULMArgs - Number of tuplip arguments on command line
 *         IndexULMArgs - Index of ULM arguments
 *         ReadNPFromFile - if true, read number of process from specified file
 * Output: NHost - Number of hosts to be used
 *         HostList - list of hosts to be used (memory is allocated in this routine)
 */

int SetupClientHostInfo(int argc, char **argv, int NULMArgs,
                        int *IndexULMArgs, int *NHosts,
                        HostName_t ** HostList, int ReadNPFromFile,
                        int **ProcessCnt, int ReadExDirFromFile,
                        DirName_t ** WorkingDirList, ExeName_t ** ExeList)
{
    char LocalHostName[ULM_MAX_HOSTNAME_LEN];
    int CommandLineHostList, IndxTPStart = 0, IndxTPEnd = 0;
    int NArgs, ExpectNArgs, n, NHostInfoFound, ExeListFound;
    int Found;
    int i, j, RetVal, cmp, cnt, cnt1, ival, ExeIndex;
    size_t len;
    struct hostent *NetEntry;
    int NSeparators = 2;
    char SeparatorList[] = { " , " };
    char **ArgList;
    FILE *InputHostList;
    DirName_t DirName;
    char buf[ULM_MAX_CONF_FILELINE_LEN];
#define MAX_TMP_STRINGS 5
    char buf1[MAX_TMP_STRINGS][ULM_MAX_CONF_FILELINE_LEN];

    /* figure out if host information is provided */
    /* the -tpsv is used ONLY to change order of hosts */
    NHostInfoFound = 0;
    ExeListFound = 0;
    for (i = 0; i < NULMArgs; i++) {
        cmp = strcmp(argv[IndexULMArgs[i]], "-tph");
        if (cmp == 0)
            NHostInfoFound = 1;
        cmp = strcmp(argv[IndexULMArgs[i]], "-tpcf");
        if (cmp == 0)
            NHostInfoFound = 1;
        cmp = strcmp(argv[IndexULMArgs[i]], "-tpexe");
        if (cmp == 0)
            ExeListFound = 1;
    }
    /* no host information specified - assume job will run on local host */
    if (NHostInfoFound == 0) {
        if (ReadNPFromFile) {
            printf(" Unable to get number of processes per host.\n");
            exit(-1);
        }
        *NHosts = 1;
        RetVal = gethostname(LocalHostName, ULM_MAX_HOSTNAME_LEN);
        if (RetVal < 0) {
            printf
                (" Error: gethostname() call failed - unable to get local host name.\n");
            perror(" gethostname() call failed ");
            exit(-1);
        }
        NetEntry = gethostbyname(LocalHostName);
        len = strlen(NetEntry->h_name);
        if (len > ULM_MAX_HOSTNAME_LEN) {
            printf
                (" Error: Host name too long for library buffer, length = %ld\n",
                 (long) len);
            exit(-1);
        }
        (*HostList) = ulm_new(HostName_t,  1);
        sprintf((char *) HostList[0], NetEntry->h_name, len);
        /* return after setting up host information */
        return 0;
    }

    /* end no host information specified on command line */
    /* check and see if command line arguments are specified for host */
    CommandLineHostList = 0;
    for (i = 0; i < NULMArgs; i++) {
        cmp = strncmp(argv[IndexULMArgs[i]], "-tph", 4);
        len = strlen(argv[IndexULMArgs[i]]);
        if ((cmp == 0) && (len == 4)) {
            CommandLineHostList = 1;
            IndxTPStart = IndexULMArgs[i] + 1;
            if (i != (NULMArgs - 1)) {
                IndxTPEnd = IndexULMArgs[i + 1] - 1;
            } else {
                if (IndxTPStart > argc) {
                    printf
                        ("Error: No hosts specified for host list (option -tph).\n");
                    exit(-1);
                }
                IndxTPEnd = argc - 1;
            }
            break;
        }
    }
    if (CommandLineHostList) {
        if (ReadNPFromFile) {
            printf(" Unable to get number of processes per host.\n");
            exit(-1);
        }
        /* parse the data */
        *NHosts = 0;
        for (i = IndxTPStart; i <= IndxTPEnd; i++) {
            NArgs =
                _ulm_ParseString(&ArgList, argv[i], NSeparators,
                                 SeparatorList);
            (*NHosts) += NArgs;
            _ulm_FreeStringMem(&ArgList, NArgs);
        }
        /* allocate space for host list, and reparse list */
        (*HostList) = ulm_new(HostName_t,  (*NHosts));
        cnt = 0;
        for (i = IndxTPStart; i <= IndxTPEnd; i++) {
            NArgs =
                _ulm_ParseString(&ArgList, argv[i], NSeparators,
                                 SeparatorList);
            for (j = 0; j < NArgs; j++) {
                NetEntry = gethostbyname(ArgList[j]);
                if (NetEntry == NULL) {
                    printf("Error: Unrecognized host name %s\n",
                           ArgList[j]);
                    perror(" gethostbyname ");
                    exit(-1);
                }
                len = strlen(NetEntry->h_name);
                if (len > ULM_MAX_HOSTNAME_LEN) {
                    printf
                        (" Error: Host name too long for library buffer, length = %ld\n",
                         (long) len);
                    exit(-1);
                }
                sprintf((char *) (*HostList)[cnt], NetEntry->h_name, len);
                cnt++;
                free(ArgList[j]);
            }
            free(ArgList);
        }
        /* done parsing command line arguments */
        return 0;
    }

    /* read specified file - if command line argument list not provided */

    /* find host list file specification */
    Found = 0;
    for (i = 0; i < NULMArgs; i++) {
        if (strncmp(argv[IndexULMArgs[i]], "-tpcf", 5) == 0) {
            Found = 1;
            break;
        }
    }
    InputHostList = fopen(argv[IndexULMArgs[i] + 1], "rb");
    if (InputHostList == NULL) {
        printf("Error: opening hostlist file: %s\n", argv[i]);
        perror(" Open Error ");
        exit(-1);
    }
    ExeIndex = 3;
    if (!ExeListFound) {
        if (!ReadExDirFromFile)
            ExeIndex--;
        if (!ReadNPFromFile)
            ExeIndex--;
    }
    cnt = 0;
    cnt1 = 0;                   /* cnt - tracks number of non-empty lines
                                   cnt1 - tracks number of hosts */
    ExpectNArgs = 1;
    if (ReadNPFromFile)
        ExpectNArgs++;
    if (ReadExDirFromFile)
        ExpectNArgs++;
    if (!ExeListFound)
        ExpectNArgs++;
    Found = 0;
    while (fgets(buf, sizeof(buf), InputHostList)) {
        /* check for comment lines */
        NArgs = _ulm_NStringArgs(buf, 3, " ,\n");
        if (NArgs == 0)
            continue;
        sscanf(buf, "%s", buf1[0]);
        if (!(buf1[0][0] == '#')) {
            /* check to see if line specifying number of hosts exists */
            NArgs = _ulm_NStringArgs(buf, 2, " =");
            if ((cnt == 0) && (NArgs == 2)) {
                NArgs = _ulm_ParseString(&ArgList, buf, 2, " =");
                if (strncmp(ArgList[0], "nhosts", 5) == 0) {
                    *NHosts = atoi(ArgList[1]);
                    if ((*NHosts) == 0) {
                        printf
                            ("Error: getting number of Hosts from host file.\n");
                        printf(" String Parsed %s\n", ArgList[1]);
                        perror(" Error in atoi ");
                        exit(-1);
                    }
                }
                free(ArgList[0]);
                free(ArgList[1]);
                free(ArgList);
                cnt++;
                Found = 1;
                continue;
            }                   /* end check on number of hosts */
            if (NArgs < ExpectNArgs) {
                printf("Missing data in configuration file.");
                printf("  %d arguments expeted, but got only %d\n",
                       ExpectNArgs, NArgs);
                printf(" Input line:: %s", buf);
                exit(-1);
            }
            n = _ulm_ParseString(&ArgList, buf, 3, " ,\n");
            NetEntry = gethostbyname(ArgList[0]);
            if (NetEntry == NULL) {
                printf("Error: Unrecognized host name %s\n", buf1[0]);
                perror(" gethostbyname ");
                exit(-1);
            }
            len = strlen(NetEntry->h_name);
            if (len > ULM_MAX_HOSTNAME_LEN) {
                printf
                    (" Error: Host name too long for library buffer, length = %ld\n",
                     (long) len);
                exit(-1);
            }

            if (ReadNPFromFile) {
                ival = atoi(ArgList[1]);
                if (ival <= 0) {
                    printf
                        ("Error: reading in number of process. \n  Input line:: %s",
                         buf1[1]);
                    exit(-1);
                }
            }
            for (i = 0; i < n; i++)
                free(ArgList[i]);
            free(ArgList);
            cnt1++;
            cnt++;
        }
        /* end check for comments */
        /* found desired number of hosts */
        if (Found && (cnt1 == *NHosts))
            break;
    }                           /* end parse file */
    if (Found && (cnt1 < *NHosts)) {
        printf("Error:  Found %d hosts in host_file, but expected %d\n",
               cnt1, *NHosts);
        exit(-1);
    }
    /* rewind and readin the data  - error already checked for */
    *NHosts = cnt1;
    fseek(InputHostList, 0, SEEK_SET);
    (*HostList) = ulm_new(HostName_t,  *NHosts);
    *ProcessCnt = ulm_new(int, *NHosts);
    /* allocate space for dirctory list */
    if (ReadExDirFromFile) {
        (*WorkingDirList) = ulm_new(DirName_t,  *NHosts);
    }
    /* allocae space for executable list */
    if (!ExeListFound) {
        (*ExeList) = ulm_new(ExeName_t,  *NHosts);
    }
    cnt = 0;
    cnt1 = 0;
    while (fgets(buf, sizeof(buf), InputHostList)) {
        NArgs = _ulm_NStringArgs(buf, 3, " ,\n");
        if (NArgs == 0)
            continue;
        /* check for comment lines */
        sscanf(buf, "%s", buf1[0]);
        if (!(buf1[0][0] == '#')) {
            /* check to see if line specifying number of hosts exists */
            if ((cnt == 0) && Found) {
                cnt++;
                continue;
            }
            n = _ulm_ParseString(&ArgList, buf, 3, " ,\n");
            NetEntry = gethostbyname(ArgList[0]);
            len = strlen(NetEntry->h_name);
            sprintf((char *) (*HostList)[cnt1], NetEntry->h_name, len);
            /* get number of contexts */
            if (ReadNPFromFile) {
                (*ProcessCnt)[cnt1] = atoi(ArgList[1]);
            }
            /* get directory information */
            if (ReadExDirFromFile) {
                if (ReadNPFromFile)
                    j = 2;
                else
                    j = 1;
                cmp = strncmp(ArgList[j], "/", 1);
                if (cmp == 0) {
                    sprintf((char *) (*WorkingDirList)[cnt1], "%s",
                            ArgList[j]);
                } else {
                    if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
                        printf("getcwd() call failed\n");
                        perror(" getcwd ");
                        exit(-1);
                    }
                    sprintf((char *) (*WorkingDirList)[cnt1], "%s/%s",
                            DirName, ArgList[j]);
                }
            }
            /* get executable name */
            if (!ExeListFound) {
                sprintf((char *) (*ExeList)[cnt1], "%s",
                        ArgList[ExeIndex]);
            }

            for (i = 0; i < NArgs; i++)
                free(ArgList[i]);
            free(ArgList);
            cnt1++;
            cnt++;
        }
        /* end check for comments */
        /* found desired number of hosts */
        if (Found && (cnt1 == (*NHosts)))
            break;
    }                           /* end parse file */

    fclose(InputHostList);

    return 0;
}
