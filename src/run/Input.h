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



#ifndef _INPUT
#define _INPUT

#include <stdio.h>
#include <stdlib.h>
#include "internal/constants.h"
#include "util/ParseString.h"

struct InputParameters_t {
    /* abreviated command line option name */
    char *AbreviatedName;
    /* name used in Database file and evnironment variable */
    char *FileName;
    /* enum used to indicate what type of input will accompany this */
    int TypeOfArgs;
    /* this function will be invoked after all input is done */
    void (*fpToExecute) (const char *ErrorString);
    /* if input is done, this function is pointed to by fpToExecute */
    void (*fpProcessInput) (const char *ErrorString);
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
void Usage(FILE *stream);
int MatchOption(char *StringToMatch, InputParameters_t *ULMInputOptions,
                int SizeOfInputOptionsDB);
void FillIntData(ParseString *InputObj, int SizeOfArray, int *Array,
                 InputParameters_t *ULMInputOptions, int OptionIndex,
                 int MinVal);
void GetDirectoryList(char *Option, DirName_t ** Array, int ArrayLength);
void FatalNoInput(const char *ErrorString);
void NoOpFunction(const char *ErrorString);
void NotImplementedFunction(const char *ErrorString);
void SetCheckArgsFalse(const char *ErrorString);
void SetOutputPrefixTrue(const char *ErrorString);
void SetQuietTrue(const char *ErrorString);

extern InputParameters_t ULMInputOptions[];
extern int SizeOfInputOptionsDB;

#endif /* _INPUT */
