/*
 * File:	util.c
 * Description:	some utilities
 *
 */

/* Standard C header files ********************************************/

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/times.h>

#include <assert.h>

/* Other header files *************************************************/

#include <ulm/ulm.h>

#include "ulmtest.h"

/* Functions **********************************************************/

/*
 * POSIX compliant clock -- this is all the resolution we need here
 */

double second(void)
{
    static long tick = 0;
    struct tms buffer;

    if (tick == 0) {
	tick = sysconf(_SC_CLK_TCK);
    }

    return ((double) times(&buffer) / (double) tick);
}

/*
 * Error checking function. Only called via the CHECK_ULM macro
 */

void check_ulm(int rc, char *expr, char *file, int line)
{
    static int self = -1;

    if (self < 0) {
	ulm_get_info(ULM_COMM_WORLD, ULM_INFO_PROCID, &self,
		     sizeof(int));
    }

    if (rc != ULM_SUCCESS) {
	fprintf(stderr, "[%d.%d] %s:%d: ULM ERROR: \'%s\' (%d))\n",
		ULM_COMM_WORLD, self, file, line, expr, rc);
	fflush(stderr);
    }
}

/*
 * Data generating and checking functions.
 */

void set_data(int self, int seq, unsigned char pattern,
	      unsigned char *buf, size_t size)
{
    int i;

    pattern += self;
    for (i = 0; i < size; i++, pattern++) {
	buf[i] = pattern ^ ((seq & 1) ? seq : ~seq);
    }
}


void check_data(int peer, int seq, unsigned char pattern,
		unsigned char *buf, size_t size, char *file, int line)
{
    int i;
    static int self = -1;

    if (self < 0) {
	ulm_get_info(ULM_COMM_WORLD, ULM_INFO_PROCID, &self,
		     sizeof(int));
    }

    pattern += peer;
    for (i = 0; i < size; i++, pattern++) {
	if (buf[i] != (pattern ^ ((seq & 1) ? seq : ~seq) & 0xFF)) {
	    fprintf(stderr,
		    "[%d.%d] %s:%d: INVALID DATA "
		    "from %d at seq %d, offset %d: 0x%02X != 0x%02X\n",
		    ULM_COMM_WORLD, self, file, line,
		    peer, seq, i,
		    buf[i], (pattern ^ ((seq & 1) ? seq : ~seq)) & 0xFF);
	    fflush(stderr);
	}
    }
}


/*
 * A simple logarithmic barrier:
 *
 *  n communicates with n +/- 2^0
 *  n communicates with n +/- 2^1
 *  etc.
 */

void barrier(int group)
{
    int nproc;
    int self;
    int shift;
    int tag_mask = 0xAA << 16;

    CHECK_ULM(ulm_get_info(group, ULM_INFO_PROCID, &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    for (shift = 1; shift < nproc; shift <<= 1) {
	ULMRequestHandle_t req;
	ULMStatus_t status;
	int fpeer = self + shift;
	int bpeer = self - shift;
	int msg;

	if (fpeer < nproc) {
	    CHECK_ULM(ulm_isend(&self,  sizeof(int), NULL, fpeer,
                                tag_mask | self, group, &req, ULM_SEND_STANDARD));
	    CHECK_ULM(ulm_wait(&req, &status));
	}
	if (bpeer >= 0) {
	    CHECK_ULM(ulm_isend(&self,  sizeof(int), NULL, bpeer,
                                tag_mask | self, group, &req, ULM_SEND_STANDARD));
	    CHECK_ULM(ulm_wait(&req, &status));
	}

	if (fpeer < nproc) {
	    CHECK_ULM(ulm_irecv(				&msg,  sizeof(int), NULL, fpeer,  tag_mask | fpeer,  group,  &req));
	    CHECK_ULM(ulm_wait(&req, &status));
	}
	if (bpeer >= 0) {
	    CHECK_ULM(ulm_irecv(				&msg,  sizeof(int), NULL, bpeer,  tag_mask | bpeer,  group,  &req));
	    CHECK_ULM(ulm_wait(&req, &status));
	}
    }
}

/*
 * printf clone prints out a message but prepends group and rank info
 */

void print_simple(int group, char *format, ...)
{
    int self;
    va_list ap;

    CHECK_ULM(ulm_get_info(group, ULM_INFO_PROCID, &self, sizeof(int)));

    va_start(ap, format);
    printf("[%d.%d] ", group, self);
    vprintf(format, ap);
    fflush(stdout);
    va_end(ap);
}

/*
 * Print status
 */

void print_status(int group, ULMStatus_t *status)
{
    int self;

    CHECK_ULM(ulm_get_info(group, ULM_INFO_PROCID, &self, sizeof(int)));

    printf("[%d.%d] "
	   "status = (tag:%d, src/dest:%d, state:%d, error:%d, size:%ld)\n",
	   group, self,
	   status->tag,
	   status->proc.source,
	   status->state, status->error, status->matchedSize);
}

/*
 * printf clone prints out a message on root of group only.
 * Also acts as a barrier.
 */

void print_root(int group, char *format, ...)
{
    int self;
    va_list ap;

    CHECK_ULM(ulm_get_info(group, ULM_INFO_PROCID, &self, sizeof(int)));

    fflush(stdout);
    barrier(group);

    if (self == 0) {
	va_start(ap, format);
	vprintf(format, ap);
	fflush(stdout);
	va_end(ap);
    }

    barrier(group);
}


/*
 * printf clone prints out a message on all members of group in order.
 * Also acts as a barrier.
 */

void print_in_order(int group, char *format, ...)
{
    va_list ap;
    int nproc;
    int self;
    int peer;
    int tag_mask = 0xCC << 16;
    ULMRequestHandle_t req;
    ULMStatus_t status;
    char msgo[8];
    char msgi[8];

    CHECK_ULM(ulm_get_info(group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    va_start(ap, format);

    if (self == 0) {

	/*
	 * Print, then tell each peer in turn to print and
	 * wait for acknowledgement.
	 */

	printf("[%d.%d] ", group, self);
	vprintf(format, ap);
	fflush(stdout);

	strncpy(msgo, "print", sizeof(msgo));

	for (peer = 1; peer < nproc; peer++) {

	    CHECK_ULM(ulm_isend(msgo,  sizeof(msgo), NULL, peer,
                                tag_mask | self, group, &req, ULM_SEND_STANDARD));
	    CHECK_ULM(ulm_wait(&req, &status));

	    CHECK_ULM(ulm_irecv(				msgi,  sizeof(msgi), NULL, peer,  tag_mask | peer,  group,  &req));
	    CHECK_ULM(ulm_wait(&req, &status));

	    assert(strncmp("ok", msgi, sizeof(msgi)) == 0);
	}

    } else {

	/*
	 * Wait until told to print, then acknowledge
	 */

	CHECK_ULM(ulm_irecv(			    msgi,  sizeof(msgi), NULL, 0,  tag_mask | 0,  group,  &req));
	CHECK_ULM(ulm_wait(&req, &status));

	assert(strncmp("print", msgi, sizeof(msgi)) == 0);

	printf("[%d.%d] ", group, self);
	vprintf(format, ap);
	fflush(stdout);

	strncpy(msgo, "ok", sizeof(msgo));

	CHECK_ULM(ulm_isend(msgo,  sizeof(msgo), NULL, 0,
                            tag_mask | self, group, &req, ULM_SEND_STANDARD));
	CHECK_ULM(ulm_wait(&req, &status));
    }

    va_end(ap);

    barrier(group);
}
