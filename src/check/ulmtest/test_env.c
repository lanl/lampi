/*
 * File:	test_env.c
 * Description:	simple print local environment
 *
 */

/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>

/* Other header files *************************************************/

#include <ulm/ulm.h>

#include "ulmtest.h"

/* Functions **********************************************************/

int	test_env(state_t *state)
{
    extern char	**environ;
    char		**envp = environ;
    char		*string;
    int		pos;

    /*
     * Trust that the environment is less than 128kb ..
     */

    string = (char *) malloc(128<<10 * sizeof(char));
    if (string == (char *) NULL) {
        return(-1);
    }

    pos = 0;
    *string = '\0';
    while (*envp) {
        pos += sprintf(string + pos, "%s\n", *envp);
        envp++;
    }
    print_in_order(state->group, "Environment:\n%s", string);

    if (string) {
        free(string);
    }

    return(0);
}
