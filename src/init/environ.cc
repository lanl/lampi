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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * LAMPI environment variables.
 *
 * LAMPI uses a variety of environment variables in its operation,
 * some for passing required information to the application, and some
 * for setting options.
 *
 * This file lists all of the environment variables (potentially) used
 * by LAMPI together with their default values, and defines functions
 * lampi_environ_init() and lampi_environ_find_XXX() for accessing the
 * environment data.
 *
 * Note that some variables are only relevant on certain platforms.
 *
 * Environment variables are treated in 4 varieties: integer, real,
 * string, and array of strings (terminated by NULL).  For the latter,
 * the default value is supplied as a colon separated concatenation of
 * the strings.
 *
 * Integer environment variables are cataloged in the struct
 * lampi_environ_integer, real in lampi_environ_real, etc.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "init/environ.h"
#include "internal/linkage.h"
#include "internal/log.h"
#include "internal/malloc.h"

extern int errno;

static struct {
    char 	*name;
    int 	default_value;
    int 	value;
} lampi_environ_integer[] = {
    { "LAMPI_VERBOSE", 0 },
    { "LAMPI_NHOSTS", 0 },
    { "LAMPI_HOSTID", 0 },
	
    /* authorization code for client/server handshake. */
    { "LAMPI_ADMIN_AUTH0", 0 },
    { "LAMPI_ADMIN_AUTH1", 0 },
    { "LAMPI_ADMIN_AUTH2", 0 },
	
    /* TCP port that mpirun is listening on. */
    { "LAMPI_ADMIN_PORT", 0 },
	
    { "LAMPI_NO_CHECK_ARGS", 0 },

    /* flag for disabling fortran layer */
    { "LAMPI_FORTRAN", 1 },

    /* flag to indicate whether to spawn locally; for debug purposes. */
    { "LAMPI_LOCAL", 0 },

    /* flag whether procs are running in LSF environ. */
    { "LAMPI_WITH_LSF", 0 },
	
    /* config info for RMS environ. */
    { "RMS_NNODES", 0 },
    { "RMS_NODEID", 0 },
	
    /* T5 config info. */
    { "T5_EVENT0", E0 },
    { "T5_EVENT1", E1 },
	
    /* BPROC info. */
    { "BPROC_RANK", -1 },
	
    { NULL }
};

static struct {
    char *name;
    double default_value;
    double value;
} lampi_environ_real[] = {
    { NULL }
};

static struct {
    char *name;
    char *default_value;
    char *value;
} lampi_environ_string[] = {
    /* Host name/IP where mpirun process is running. */
    { "LAMPI_ADMIN_IP", "" },
	
    /* ??? */
    { "LSB_MCPU_HOSTS", "" },

    /* BPROC info. */
    { "NODES", "" },
	
    { NULL }
};


static struct  {
    char *name;
    char *default_value;
    char **value;
} lampi_environ_string_array[] = {
    { "LAMPI_HOSTLIST", "sally:joe:fred" },
    { "LAMPI_CPULIST", "" },
    { NULL }
};

/*
 * Utility function:
 *
 * Get a long value from the environment, or supply the default value
 * if the environment variable is not set (or is not an integer, or is
 * out of range).
 */
static int getenv_int(const char *name, int default_value)
{
    int value;
    char *tail;

    if (getenv(name) == NULL) {
        return default_value;
    }

    errno = 0;
    value = (int) strtol(getenv(name), &tail, 0);
    if (errno || *tail != '\0') {
        return default_value;
    }

    return value;
}


/*
 * Utility function:
 *
 * Get a double value from the environment, or supply the default
 * value if the environment variable is not set (or is not an integer,
 * or is out of range).
 */
static double getenv_double(const char *name, double default_value)
{
    double value;
    char *tail;

    if (getenv(name) == NULL) {
        return default_value;
    }

    errno = 0;
    value = strtod(getenv(name), &tail);
    if (errno || *tail != '\0') {
        return default_value;
    }

    return value;
}


/*
 * Utility function:
 *
 * Get a string value from the environment, or supply the default
 * value if the environment variable is not set.  The string is
 * allocated using malloc(), and so should be freed using free()
 */
static char *getenv_string(const char *name, const char *default_value)
{
    char *string;
    char *value;
    size_t len;

    value = getenv(name);
    if (value == NULL) {
        value = (char *) default_value;
    }
    if (value == NULL) {
        return NULL;
    }

    len = strlen(value);
    string = (char *) ulm_malloc(len + 1);
    if (!string) {
        return NULL;
    }
    strcpy(string, value);

    return string;
}


/*
 * Utility function:
 *
 * Get a array of strings from a path-like (i.e. colon-separated
 * valued) environment variable, or supply the default value if the
 * environment variable is not set.  The array is terminated with a
 * null pointer.  The array and the strings are allocated using
 * malloc(), and so should be freed using free() as in:
 *
 *   for (i = 0; array[i]; i++) {
 *       free(array[i]);
 *   }
 *   free(array);
 */
static char **getenv_string_array(const char *name,
                                  const char *default_value)
{
    char **array;
    char *value;
    char *s;
    int i;
    int j;
    int len;
    int nitem;

    value = getenv(name);
    if (value == NULL) {
        value = (char *) default_value;
    }
    if (value == NULL) {
        return NULL;
    }
    for (s = value, nitem = 1; *s != '\0'; s++) {
        if (*s == ':') {
            nitem++;
        }
    }

    array = (char **) ulm_malloc(sizeof(char *) * (nitem + 1));
    if (!array) {
        return NULL;
    }

    s = value;
    for (i = 0; i < nitem; i++) {
        len = strcspn(s, ":");
        array[i] = (char *) ulm_malloc(len + 1);
        if (!array[i]) {
            return NULL;
        }
        for (j = 0; j < len; j++) {
            array[i][j] = *s++;
        }
        array[i][len] = '\0';
        s++;
    }
    array[nitem] = NULL;

    return array;
}

CDECL_BEGIN

/*
 * Initialize the LAMPI environment struct from the environment
 */
void lampi_environ_init(void)
{
    int i;

    for (i = 0; lampi_environ_integer[i].name; i++) {
        lampi_environ_integer[i].value =
            getenv_int(lampi_environ_integer[i].name,
                       lampi_environ_integer[i].default_value);
    }

    for (i = 0; lampi_environ_real[i].name; i++) {
        lampi_environ_real[i].value =
            getenv_double(lampi_environ_real[i].name,
                          lampi_environ_real[i].default_value);
    }

    for (i = 0; lampi_environ_string[i].name; i++) {
        lampi_environ_string[i].value =
            getenv_string(lampi_environ_string[i].name,
                          lampi_environ_string[i].default_value);
    }

    for (i = 0; lampi_environ_string_array[i].name; i++) {
        lampi_environ_string_array[i].value =
            getenv_string_array(lampi_environ_string_array[i].name,
                                lampi_environ_string_array[i].
                                default_value);
    }
}


/*
 * Find an integer value in the LAMPI environment.
 */
int lampi_environ_find_integer(const char *name, int *eval)
{
    int i;
    int ret = LAMPI_ENV_ERR_NOT_FOUND;

    if (NULL == name)
        return LAMPI_ENV_ERR_NULL_NAME;

    *eval = 0;
    for (i = 0; lampi_environ_integer[i].name; i++) {
        if (strcmp(name, lampi_environ_integer[i].name) == 0) {
            *eval = lampi_environ_integer[i].value;
            ret = 0;
            break;
        }
    }

    return ret;
}


/*
 * Find a real value in the LAMPI environment.
 */
int lampi_environ_find_real(const char *name, double *eval)
{
    int i;
    int ret = LAMPI_ENV_ERR_NOT_FOUND;

    if (NULL == name)
        return LAMPI_ENV_ERR_NULL_NAME;

    *eval = 0.0;
    for (i = 0; lampi_environ_real[i].name; i++) {
        if (strcmp(name, lampi_environ_real[i].name) == 0) {
            *eval = lampi_environ_real[i].value;
            ret = 0;
            break;
        }
    }

    return ret;
}


/*
 * Find a string value in the LAMPI environment.
 */
int lampi_environ_find_string(const char *name, char **eval)
{
    int i;
    int ret = LAMPI_ENV_ERR_NOT_FOUND;

    if (NULL == name)
        return LAMPI_ENV_ERR_NULL_NAME;

    *eval = NULL;
    for (i = 0; lampi_environ_string[i].name; i++) {
        if (strcmp(name, lampi_environ_string[i].name) == 0) {
            *eval = lampi_environ_string[i].value;
            ret = 0;
            break;
        }
    }

    return ret;
}


/*
 * Find a string array value in the LAMPI environment.
 */
int lampi_environ_find_string_array(const char *name, char ***eval)
{
    int ret = LAMPI_ENV_ERR_NOT_FOUND;
    int i;

    if (NULL == name)
        return LAMPI_ENV_ERR_NULL_NAME;

    *eval = NULL;
    for (i = 0; lampi_environ_string_array[i].name; i++) {
        if (strcmp(name, lampi_environ_string_array[i].name) == 0) {
            *eval = lampi_environ_string_array[i].value;
            ret = 0;
            break;
        }
    }


    return ret;
}


/*
 * Dump out the LAMPI environment.
 */
void lampi_environ_dump(void)
{
    char **p;
    int i;

    for (i = 0; lampi_environ_integer[i].name; i++) {
        fprintf(stderr, "lampi_environ_dump: %s=%d [default=%d]\n",
                lampi_environ_integer[i].name,
                lampi_environ_integer[i].value,
                lampi_environ_integer[i].default_value);
    }

    for (i = 0; lampi_environ_real[i].name; i++) {
        fprintf(stderr, "lampi_environ_dump: %s=%lf [default=%lf]\n",
                lampi_environ_real[i].name,
                lampi_environ_real[i].value,
                lampi_environ_real[i].default_value);
    }

    for (i = 0; lampi_environ_string[i].name; i++) {
        fprintf(stderr, "lampi_environ_dump: %s=\"%s\" [default=\"%s\"]\n",
                lampi_environ_string[i].name,
                lampi_environ_string[i].value,
                lampi_environ_string[i].default_value);
    }

    for (i = 0; lampi_environ_string_array[i].name; i++) {
        fprintf(stderr, "lampi_environ_dump: %s=\"",
                lampi_environ_string_array[i].name);
        if (lampi_environ_string_array[i].value) {
            for (p = lampi_environ_string_array[i].value; *p != NULL; p++) {
                fprintf(stderr, "%s:", *p);
            }
            fprintf(stderr, "\b\"");
        } else {
            fprintf(stderr, "NULL");
        }
        fprintf(stderr, " [default=\"%s\"]\n",
               lampi_environ_string_array[i].default_value);
    }
    fflush(stderr);
}

CDECL_END

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
    int i;
    char **p;

    printf("Initializing ...\n");
    lampi_environ_init();

    printf("Dumping ...\n");
    lampi_environ_dump();

    printf("Finding ...\n");

    for (i = 0; lampi_environ_integer[i].name; i++) {
        printf("%s=%ld\n", lampi_environ_integer[i].name,
               lampi_environ_find_integer(lampi_environ_integer[i].name));
    }

    for (i = 0; lampi_environ_real[i].name; i++) {
        printf("%s=%lf\n", lampi_environ_real[i].name,
               lampi_environ_find_real(lampi_environ_real[i].name));
    }

    for (i = 0; lampi_environ_string[i].name; i++) {
        printf("%s=\"%s\"\n", lampi_environ_string[i].name,
               lampi_environ_find_string(lampi_environ_string[i].name));
    }

    for (i = 0; lampi_environ_string_array[i].name; i++) {
        printf("%s = \"", lampi_environ_string_array[i].name);
        p = lampi_environ_find_string_array(lampi_environ_string_array[i].
                                            name);
        if (p) {
            for (; *p != NULL; p++) {
                printf("%s:", *p);
            }
            printf("\b\"\n");
        } else {
            printf("NULL\n");
        }
    }

    i = lampi_environ_find_integer("NO_SUCH_VARIABLE");

    return EXIT_SUCCESS;
}

#endif                          /* TEST_PROGRAM */
