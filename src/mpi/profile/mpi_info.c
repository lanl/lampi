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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mpi_profile.h"
#include "mpi_info.h"
//#include "bitops.h"


#define BitSet(arg,posn) ((arg) | (1L << (posn)))
#define BitClr(arg,posn) ((arg) & ~(1L << (posn)))
#define BitFlp(arg,posn) ((arg) ^ (1L << (posn)))
#define BitGet(arg,posn) ((arg) & (1L << (posn)))

char *strsep(char **stringp, const char *delim);
int main(int argc, char *argv[])
{
    FILE *fp;
    int field = 0;
    int data = INDDATA;
    int i, j, k, l;
    int nitems = 1;
    int fieldFlag = 0;
    int funcFlag = 0;
    int num_tokens = 0;
    int num_ptokens = 0;
    int num_ftokens = 0;
    int howManyProfiles = 4;
    int ptokens[1024];
    int retCode;
    char lhs[1024];
    char rhs[1024];
    char fileName[128];
    char queryLine[128];
    char whichField;
    char *tokens[1024];
    char *ftokens[1024];
    char *buffer;
    char *rootFile = "ulmProfile.";
    char *p;
    int func;
    MPI_Stats *profileData;

    if (argc < 2) {
	fprintf(stderr, "Usage:  a.out #_of_data_files\n");
	exit(-1);
    }
    howManyProfiles = atoi(argv[1]);
    profileData = (MPI_Stats *) mallocWrapper(sizeof(MPI_Stats) * howManyProfiles);
    if (profileData == NULL)
	fprintf(stderr, "Error\n");

    // Load all the data files. that where generated with
    // the mpi profiling interface
    for (i = 0; i < howManyProfiles; i++) {
	sprintf(fileName, "%s%.3i", rootFile, i);
	fp = fopen(fileName, "r");
	if (fp == NULL) {
	    fprintf(stderr, "Unable to open the profile.data\n");
	}
	fread((void *) &(profileData[i]), (sizeof(MPI_Stats)), nitems, fp);
	profileData[i].numberOfFunc = NUMFUNCS;
    }

    // tokenize the query line.  The query line is composed of
    // three parts. p=1,2,3;field=all;data=cum;
    while (1) {
	// reinit all the variables;
	num_tokens = 0;
	num_ftokens = 0;
	num_ptokens = 0;;

	printf(">>");
	scanf("%s", queryLine);
	buffer = queryLine;

	while (buffer != NULL && *buffer != 0) {
	    tokens[num_tokens] = strsep(&buffer, ";");
	    num_tokens++;
	}
	if ((strcmp(queryLine, "q") == 0) || strcmp(queryLine, "quit") == 0) {
	    exit(-1);
	}
	// process tokesn
	for (i = 0; i < num_tokens; i++) {
	    whichField = tokens[i][0];


	    //  We check the first character of the
	    //  token to decide what type of processing
	    //   we will do

	    if (whichField == 'p') {
		sscanf(tokens[i], "%c=%s", rhs, lhs);
		p = lhs;
		while (p != NULL && *p != 0) {
		    ptokens[num_ptokens] = atoi(strsep(&p, ","));
		    num_ptokens++;
		}
		if (strcmp(lhs, "all") == 0) {
		    num_ptokens = 0;
		    for (k = 0; k < howManyProfiles; k++) {
			ptokens[k] = k;
			num_ptokens++;
		    }
		}
	    }
	    // parsing the data field.  The data field
	    // determines whether we want cumulative
	    // or individual data (that is we
	    // enumerate indiviudally through the selected
	    // process)

	    if (whichField == 'd') {
		sscanf(tokens[i], "data=%s", lhs);
		if (strcmp(lhs, "cum") == 0)
		    data = CUMDATA;
		else if (strcmp(lhs, "ind") == 0)
		    data = INDDATA;
		else {
		    printf("Error in data field\n");
		    exit(-1);
		}
	    }			// whichField == 'd'

	    // parsig the field field.. The field field
	    // determines with filed of the funcInfo struct
	    // to print...

	    if (whichField == 'f') {
		if (sscanf(tokens[i], "field=%s", lhs) != 0)
		    fieldFlag = 1;
		if (fieldFlag != 1) {
		    if (sscanf(tokens[i], "func=%s", lhs) != 0)
			funcFlag = 1;
		}
		num_ftokens = 0;
		if (fieldFlag == 1) {
		    p = lhs;
		    while (p != NULL && *p != 0) {
			ftokens[num_ftokens] = strsep(&p, ",");
			if (strcmp(ftokens[num_ftokens], "all") == 0)
			    field = BitSet(field, FIELD_ALL);
			else if (strcmp(ftokens[num_ftokens], "event") == 0)
			    field = BitSet(field, FIELD_EVENT);
			else if (strcmp(ftokens[num_ftokens], "timesnt") == 0)
			    field = BitSet(field, FIELD_TIMESC);
			else if (strcmp(ftokens[num_ftokens], "bytessent") == 0)
			    field = BitSet(field, FIELD_BYTESENT);
			else if (strcmp(ftokens[num_ftokens], "bytesrecv") == 0)
			    field = BitSet(field, FIELD_BYTESRECV);
			else if (strcmp(ftokens[num_ftokens], "Op") == 0)
			    field = BitSet(field, FIELD_OP);
			else
			    printf("field not matched\n");
			fieldFlag = 0;
			funcFlag  = 0;
			num_ftokens++;
		    }
		}
		if (funcFlag == 1) {
		    p = lhs;
		    while (p != NULL && *p != 0) {
			ftokens[num_ftokens] = strsep(&p, ",");
			for (k = 0; k < NUMFUNCS; k++) {
			    if (strcmp(ftokens[num_ftokens], funcList[k]) == 0)
				func = BitSet(func, k);
			}
			fieldFlag = 0;
			funcFlag = 0;
			if (strcmp(ftokens[num_ftokens], "all") == 0) {
                            for (k=0;k<NUMFUNCS;k++)
                                func = BitSet(func,k);
                            fieldFlag = 0;
                            funcFlag = 0;
                            break;
			}
			num_ftokens++;
		    }
		}
	    }
	    if (whichField == 'h') {
		printf("help\n");
	    }
	}			// for (i=0;i<num_tokens;.....


	//  hopefully the query line has been processed
	//  now it's time to do something with the
	//  calculate cumulative data for the
	//  processor selected;
	if (data == CUMDATA) {
	    for (i = 0; i < num_ptokens; i++) {
		k = ptokens[i];
		for (j = 0; j < NUMFUNCS; j++) {
		    cumData.funcInfo[j].timesCalled += profileData[k].funcInfo[j].timesCalled;
		    for (l = 0; l < 4; l++)
			cumData.funcInfo[j].bins[l] += profileData[k].funcInfo[j].bins[l];
		    cumData.funcInfo[j].bytesSent += profileData[k].funcInfo[j].bytesSent;
		    cumData.funcInfo[j].bytesRecv += profileData[k].funcInfo[j].bytesRecv;
		    cumData.funcInfo[j].cumTime += profileData[k].funcInfo[j].cumTime;
		    for (l = 0; l< 6; l++)
			cumData.funcInfo[j].op[l] +=profileData[k].funcInfo[j].bins[l];

		}
	    }

	    printf("data= %i, field= %i, func= %i\n",data,field,func);
	    printFuncInfo(&cumData, data, field, func);
	}			// end of cumulative data

	if (data == INDDATA) {
	    for (i = 0; i < num_ptokens; i++) {
		j = ptokens[i];
		printFuncInfo(&(profileData[j]), data, field, func);
	    }
	}
    }				// end of while loop
}

int printFuncInfo(MPI_Stats * stats, int data, int field, int funcdata)
{
    int timesCalled = 0;
    int word;
    int selectedFields;
    int funk;
    int bins = 0;
    int i = 0;
    int j = 0;
    int bytesSent = 0;
    int bytesRecv = 0;
    int event0 = 0;
    int event1 = 0;
    int fieldCounter = 0;
    int op = 0;
    long long counter0;
    long long counter1;
    char *p[10];

    for (i = 0; i < NUMFUNCS; i++) {
	word = BitGet(field, i);
	if (word != 0) {
	    selectedFields = i;
	    switch (selectedFields) {
	    case FIELD_ALL:
		timesCalled = 1;
		bins = 1;
		bytesSent = 1;
		bytesRecv = 1;
		event0 = 0;
		event1 = 0;
		op     = 0;
                break;
	    case FIELD_TIMESC:
		timesCalled = 1;
		break;
	    case FIELD_BYTESENT:
		bytesSent = 1;
		break;
	    case FIELD_BYTESRECV:
		bytesRecv = 1;
		break;
	    case FIELD_EVENT:
		event0 = 1;
		event1 = 1;
		break;
	    case FIELD_OP:
		op = 1;
		break;
	    default:
		;
	    }
	}
    }


    for (j = 0; j < NUMFUNCS; j++) {
	word = BitGet(funcdata, j);
	if (word != 0) {
	    funk = j;
	    {
		if (data == INDDATA) {
		    printf("===============================================\n");
		    printf("Examining proc(s)  %i\n", stats->myTask);
		    printf("Examining func %s\n",funcList[funk]);
		    printf("===============================================\n");
		}
		if (data == CUMDATA) {
		    printf("=============================================\n");
		    printf("Cumulative data\n");
                    printf("=============================================\n");
		}
		printf("Examining func %s\n",funcList[funk]);
		if (timesCalled == 1)
		    printf("timesCalled = %i\n", stats->funcInfo[funk].timesCalled);

		if (bins == 1) {
		    for (i = 0; i < 5; i++)
			printf("bin[%i] = %i\n", i, stats->funcInfo[funk].bins[i]);
		}
		if (bytesSent == 1)
		    printf("bytesSent = %i\n", stats->funcInfo[funk].bytesSent);
		if (bytesRecv == 1)
		    printf("bytesRecv = %i\n", stats->funcInfo[funk].bytesRecv);

		if (event0 == 1)
		    printf("Event counter 0 = %i; value = %i\n", stats->e0, (int) (stats->counter0));

		if (event1 == 1)
		    printf("Event counter 1 = %i; value = %i\n", stats->e1, (int) (stats->counter1));

                printf("Cumulative time = %f\n",(double)(stats->funcInfo[funk].cumTime));

		if (op == 1) {
		    for (i = 0; i < 6; i++) {
			printf("Op %s= %i\n",opList[i],stats->funcInfo[funk].op[i]);

		    }
		}
	    }
	}
    }


    //	reset
    stats->funcInfo[funk].timesCalled = 0;
    stats->funcInfo[funk].bytesSent   = 0;
    stats->funcInfo[funk].bytesRecv	  = 0;
    stats->e0			  = 0;
    stats->e1			  = 0;
    for (j = 0 ;j<NUMFUNCS;j++)
	for (i = 0; i < 5; i++)
            stats->funcInfo[funk].bins[i] = 0;
}

char *strsep(char **stringp, const char *delim)
{
    char *res;
    if (!stringp || !*stringp || !**stringp)
	return (char *) 0;

    res = *stringp;
    while (**stringp && !strchr(delim, **stringp))
	(*stringp)++;

    if (**stringp) {
	**stringp = '\0';
	(*stringp)++;
    }
    return res;
}
