/*
 * Copyright 2002-2003. The Regents of the University of
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int integer_compare(const void *a, const void *b)
{
    return *((int *) a) -  *((int *) b);
}

/*
 * parseNodeString:
 *
 * Convert a node set specification string of the form
 * 4,5,9,11-15,23-67 into a node count and an integer array listing
 * the node numbers.
 * 
 * Arguments:
 *   nodestring -- specifies a node set (e.g. 0,2,4-11,3,4)
 *   sort       -- should list be sorted?
 *   uniq       -- should list be uniquely sorted?
 *   nnodesp    -- pointer to integer number of nodes in the set
 *   nodesp     -- pointer / integer array for the node numbers
 *
 * Return:
 *   0 on success, -1 otherwise
 * 
 *Comments:
 *   The nodesp array is malloced and so should be freed by the caller.
 */
int parse_node_string(const char *string, int sort, int uniq,
                      int *nnodesp, int **nodesp)
                    
{
    char *s;
    char *temp = NULL;
    char *token;
    int *nodes = NULL;
    int i;
    int lower;
    int nnodes = 0;
    int nscanned;
    int status = 0;
    int upper;

    temp = strdup(string);
    if (!temp) {
        status = -1;
    }

    /* first pass: count the nodes */
    if (status == 0) {
        s = (char *) temp;
        nnodes = 0;
        while (status == 0 && s) {
            token = strsep(&s, ",");
            nscanned = sscanf(token, "%d-%d", &lower, &upper);
            if (nscanned == 1) {
                nnodes += 1;
            } else if (nscanned == 2) {
                if (upper <= lower) {
                    status = -1;
                }
                nnodes += upper - lower + 1;
            } else {
                return -1;
            }
        }
    }

    /* allocate nodes array */
    if (status == 0 && nnodes) {
        nodes = (int *) calloc(nnodes, sizeof(int));
        if (nodes == NULL) {
            status = -1;
        }
    }

    /* second pass: fill in the array */
    if (status == 0 && nnodes) {
        strcpy(temp, string);
        s = (char *) temp;
        nnodes = 0;
        while (status == 0 && s) {
            token = strsep(&s, ",");
            nscanned = sscanf(token, "%d-%d", &lower, &upper);
            if (nscanned == 1) {
                nodes[nnodes++] = lower;
            } else if (nscanned == 2) {
                for (i = lower; i <= upper; i++) {
                    nodes[nnodes++] = i;
                }
            } else {
                status = -1;
            }
        }
    }

    /* optionally sort the array */
    if (sort && status == 0 && nnodes) {
        qsort(nodes, nnodes, sizeof(int), integer_compare);
    }

    /* uniquify the array */
    if (sort && uniq && status == 0 && nnodes) {
        int i, j;

        i = 1;
        while (i < nnodes) {
            if (nodes[i] == nodes[i - 1]) {
                for (j = i + 1; j < nnodes; j++) {
                    nodes[j - 1] = nodes[j];
                }
                nnodes--;
            } else {
                i++;
            }
        }
    }

    if (status != 0) {
        nnodes = 0;
        if (nodes) {
            free(nodes);
        }
        nodes = NULL;
    }

    *nnodesp = nnodes;
    *nodesp = nodes;
    if (temp) {
        free(temp);
    }

    return status;
}

#ifndef DISABLE_UNIT_TEST

#include <getopt.h>
#include <stdlib.h>

static void usage(char *name)
{
    printf("Usage: %s [OPTIONS] STRING\n\n"
           "Options:\n"
           "    -h    this help message\n"
           "    -s    sort the node list\n"
           "    -u    uniquely sort the node list\n"
           "\n"
           "Parse a node string (e.g. \"0,3,5-8,42\") into an integer array.\n\n",
           name);
}


int utest_parse_node_string(int argc, char *argv[])
{
    int nnodes;
    int *nodes;
    int i;
    int sort;
    int uniq;
    int c;

    sort = 0;
    uniq = 0;

    while ((c = getopt(argc, argv, "hsu")) != -1) {
        switch (c) {
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
            break;
        case 's':
            sort = 1;
            break;
        case 'u':
            sort = 1;
            uniq = 1;
            break;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind != argc - 1) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_node_string(argv[optind], sort, uniq, &nnodes, &nodes)) {
        printf("string %s is bad\n", argv[optind]);
        return EXIT_FAILURE;
    }

    printf("node string = %s\n", argv[optind]);
    printf("nnodes = %d\nnodes =", nnodes);
    for (i = 0; i < nnodes; i++) {
        printf(" %d", nodes[i]);
    }
    printf("\n");

    return EXIT_SUCCESS;
}

#endif
