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

#include "run/parse_cmdln_bproc.h"

#if ENABLE_BPROC
static struct option long_opts[] = {
    {"debug", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {"nhost", required_argument, 0, 'N'},
    {"nprocs", required_argument, 0, 'n'},
    {"define", required_argument, 0, 'x'},
    {"env", required_argument, 0, 'y'},
    {"host", required_argument, 0, 'z'},
    {0, 0, 0, 0}
};


static char *short_opts = "dhvN:n:";
int debugSet = 0;
#define IF_DEBUG(X,Y)	if (X == 1)\
			      Y;
void print_help()
{
    printf("Command line arguments to mpirun:\n"
           "\t -v  --verbose            Verbose debug output (must be first argument)\n"
           "\t -h  --help		Print a brief help message and exit\n"
           "\t -N  --nhost=NHOSTS	Number of hosts to use.\n"
           "\t -n  --nprocs=NPROCS	Number of processes to start\n"
           "\t --define VAR=VAL         Define an an architecture specific variable\n"
           "\t --env VAR=VAL            Define an environment variable\n"
           "\t --host=HOSTAME[:executable[args]]\n" "\n");
}
#endif
int parse_cmdln_bproc(int argc, char **argv, ULMRunParams_t *parameters)
{
#if ENABLE_BPROC
    int opt_index = 0;
    int c;
    int debugSet = 0;
    /* parse through command line */
    while (1) {
        c = getopt_long(argc, argv, short_opts, long_opts, &opt_index);

        /* check to see if we have no more options */
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'N':
            IF_DEBUG(debugSet, printf("Hosts: %s\n", optarg));
            break;
        case 'h':
            print_help();
            return 1;
            break;
        case 'n':
            IF_DEBUG(debugSet, printf("nprocs: %s\n", optarg));

            break;
        case 'e':
            printf("exe: %s\n", optarg);
            break;
        case 'v':
            debugSet = 1;
            printf("Debug on\n");
            break;
        case 'x':
            IF_DEBUG(debugSet, printf("define %s\n", optarg));
            break;
        case 'y':
            IF_DEBUG(debugSet, printf("env %s\n", optarg));
            break;
        case 'z':
            IF_DEBUG(debugSet, printf("host %s\n", optarg));
            break;
        default:
            printf("Error: parsing argument %d\n", c);
            return 0;
        }
    }

    /* check for extra arguments */
    if (optind < argc) {
        printf("Extra arguments on command line\n");
        print_help();
        return 0;
    }
#endif
    return 1;
}
