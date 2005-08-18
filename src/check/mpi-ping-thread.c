/*
 * MPI ping program, with threads.
 *
 * Patterned after the example in the Quadrics documentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <pthread.h>

#include "mpi.h"

struct process_state {
    int blocking;
    int check;
    int inc_bytes;
    int max_bytes;
    int min_bytes;
    int nthread;
    int overlap;
    int reps;
    int warmup;
    int thread;
};

struct thread_state {
    struct process_state *pstate;
    pthread_t thread;
    int rank;
};


static void thread_barrier(int thread_count)
{
    static int seqno = 0;
    static int threads_entered = 0;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    int start_seqno;

    pthread_mutex_lock(&mutex);

    start_seqno = seqno;
    threads_entered++;

    if (threads_entered == thread_count) {

        /*
         * barrier condition met:
         * release mutex, restart waiting threads
         */

        threads_entered = 0;
        seqno++;
        pthread_cond_broadcast(&cond);

    } else {

        /*
         * barrier condition not met:
         * release the mutex, wait for condition
         */

        while (start_seqno == seqno) {
            pthread_cond_wait(&cond, &mutex);
        }
    }

    pthread_mutex_unlock(&mutex);
}


static void global_barrier(struct thread_state *tstate)
{
    thread_barrier(tstate->pstate->nthread);
    if (tstate->rank == 0) {
        MPI_Barrier(MPI_COMM_WORLD);
    }
    thread_barrier(tstate->pstate->nthread);
}


static int str2size(char *str)
{
    int size;
    char mod[32];

    switch (sscanf(str, "%d%1[mMkK]", &size, mod)) {
    case 1:
        return (size);

    case 2:
        switch (*mod) {
        case 'm':
        case 'M':
            return (size << 20);

        case 'k':
        case 'K':
            return (size << 10);

        default:
            return (size);
        }

    default:
        return (-1);
    }
}


static void usage(void)
{
    fprintf(stderr,
            "Usage: mpi-ping [flags] <min bytes> [<max bytes>] [<inc bytes>]\n"
            "       mpi-ping -h\n");
    exit(EXIT_FAILURE);
}


static void help(void)
{
    printf("Usage: mpi-ping-thread "
           "[flags] <min bytes> [<max bytes>] [<inc bytes>]\n"
           "\n" "   Flags may be any of\n"
           "      -B                use blocking send/recv\n"
           "      -C                check data\n"
           "      -O                overlapping pings\n"
           "      -T number         number of threads per process\n"
           "      -W                perform warm-up phase\n"
           "      -r number         repetitions to time\n"
           "      -h                print this info\n" "\n"
           "   Numbers may be postfixed with 'k' or 'm'\n\n");

    exit(EXIT_SUCCESS);
}


void *threadmain(void *ptr)
{
    MPI_Request recv_request;
    MPI_Request send_request;
    MPI_Status status;
    char *rbuf;
    char *tbuf;
    int bytes;
    int i;
    int nproc;
    int peer;
    int proc;
    int r;
    int tag;
    struct thread_state *tstate;
    struct process_state *pstate;

    tstate = (struct thread_state *) ptr;
    pstate = tstate->pstate;
    tag = 0x666 + tstate->rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &proc);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    if (0) {
        sleep(10 * tstate->rank);
    }

    peer = proc ^ 1;

    if ((peer < nproc) && (peer & 1)) {
        printf("proc %d, thread %d pings proc %d thread %d\n",
               proc, tstate->rank, peer, tstate->rank);
        fflush(stdout);
    }

    if ((rbuf = (char *) malloc(pstate->max_bytes ? pstate->max_bytes : 8))
        == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if ((tbuf = (char *) malloc(pstate->max_bytes ? pstate->max_bytes : 8))
        == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (pstate->check) {
        for (i = 0; i < pstate->max_bytes; i++) {
            tbuf[i] = i & 255;
            rbuf[i] = 0;
        }
    }

    global_barrier(tstate);

    if (pstate->warmup) {

        if (proc == 0 && tstate->rank == 0) {
            puts("warm-up phase");
            fflush(stdout);
        }

        for (r = 0; r < pstate->reps; r++) {
            if (peer >= nproc) {
                break;
            }
            MPI_Irecv(rbuf, pstate->max_bytes, MPI_BYTE,
                      peer, tag, MPI_COMM_WORLD, &recv_request);
            MPI_Isend(tbuf, pstate->max_bytes, MPI_BYTE,
                      peer, tag, MPI_COMM_WORLD, &send_request);
            MPI_Wait(&send_request, &status);
            MPI_Wait(&recv_request, &status);
        }

        if (proc == 0) {
            puts("warm-up phase done");
            fflush(stdout);
        }
    }

    global_barrier(tstate);

    /*
     * Main loop
     */

    for (bytes = pstate->min_bytes; bytes <= pstate->max_bytes;
         bytes = pstate->inc_bytes ?
             bytes + pstate->inc_bytes : bytes ? 2 * bytes : 1) {

        double t = 0.0;
        double tv[2];

        r = pstate->reps;

        global_barrier(tstate);

        if (peer < nproc) {

            if (pstate->overlap) {

                /*
                 * MPI_Isend / MPI_Irecv overlapping ping-pong
                 */

                tv[0] = MPI_Wtime();

                for (r = 0; r < pstate->reps; r++) {

                    MPI_Irecv(rbuf, bytes, MPI_BYTE, peer, tag,
                              MPI_COMM_WORLD, &recv_request);
                    MPI_Isend(tbuf, bytes, MPI_BYTE, peer, tag,
                              MPI_COMM_WORLD, &send_request);
                    MPI_Wait(&send_request, &status);
                    MPI_Wait(&recv_request, &status);

                    if (pstate->check) {
                        for (i = 0; i < bytes; i++) {
                            if (rbuf[i] != (i & 255)) {
                                puts("mpi-ping: Error: Invalid data received");
                            }
                            rbuf[i] = 0;
                        }
                    }
                }

                tv[1] = MPI_Wtime();

            } else if (pstate->blocking) {

                /*
                 * MPI_Send / MPI_Recv ping-pong
                 */

                tv[0] = MPI_Wtime();

                if (peer < nproc) {
                    if (proc & 1) {
                        r--;
                        MPI_Recv(rbuf, bytes, MPI_BYTE, peer, tag,
                                 MPI_COMM_WORLD, &status);

                        if (pstate->check) {
                            for (i = 0; i < bytes; i++) {
                                if (rbuf[i] != (i & 255)) {
                                    puts("mpi-ping: Error: "
                                         "Invalid data received");
                                }
                                rbuf[i] = 0;
                            }
                        }
                    }

                    while (r-- > 0) {

                        MPI_Send(tbuf, bytes, MPI_BYTE, peer, tag,
                                 MPI_COMM_WORLD);
                        MPI_Recv(rbuf, bytes, MPI_BYTE, peer, tag,
                                 MPI_COMM_WORLD, &status);

                        if (pstate->check) {
                            for (i = 0; i < bytes; i++) {
                                if (rbuf[i] != (i & 255)) {
                                    puts("mpi-ping: Error: "
                                         "Invalid data received");
                                }
                                rbuf[i] = 0;
                            }
                        }
                    }

                    if (proc & 1) {
                        MPI_Send(tbuf, bytes, MPI_BYTE, peer, tag,
                                 MPI_COMM_WORLD);
                    }
                }

                tv[1] = MPI_Wtime();

            } else {

                /*
                 * MPI_Isend / MPI_Irecv ping-pong
                 */

                tv[0] = MPI_Wtime();

                if (peer < nproc) {
                    if (proc & 1) {
                        r--;
                        MPI_Irecv(rbuf, bytes, MPI_BYTE, peer, tag,
                                  MPI_COMM_WORLD, &recv_request);
                        MPI_Wait(&recv_request, &status);

                        if (pstate->check) {
                            for (i = 0; i < bytes; i++) {
                                if (rbuf[i] != (i & 255)) {
                                    puts("mpi-ping: Error: "
                                         "Invalid data received");
                                }
                                rbuf[i] = 0;
                            }
                        }
                    }

                    while (r-- > 0) {

                        MPI_Isend(tbuf, bytes, MPI_BYTE, peer, tag,
                                  MPI_COMM_WORLD, &send_request);
                        MPI_Wait(&send_request, &status);
                        MPI_Irecv(rbuf, bytes, MPI_BYTE, peer, tag,
                                  MPI_COMM_WORLD, &recv_request);
                        MPI_Wait(&recv_request, &status);

                        if (pstate->check) {
                            for (i = 0; i < bytes; i++) {
                                if (rbuf[i] != (i & 255)) {
                                    puts("mpi-ping: Error: "
                                         "Invalid data received");
                                }
                                rbuf[i] = 0;
                            }
                        }
                    }

                    if (proc & 1) {
                        MPI_Isend(tbuf, bytes, MPI_BYTE, peer, tag,
                                  MPI_COMM_WORLD, &send_request);
                        MPI_Wait(&send_request, &status);
                    }
                }

                tv[1] = MPI_Wtime();
            }

            /*
             * Calculate time interval in useconds (half round trip)
             */

            t = (tv[1] - tv[0]) * 1000000.0 / (2 * pstate->reps);

        }

        global_barrier(tstate);

        if ((peer < nproc) && (peer & 1)) {
            printf("%3d pinged %3d: %8d bytes %9.2f uSec %8.2f MB/s\n",
                   proc, peer, bytes, t, bytes / (t));
            fflush(stdout);
        }
    }

    return NULL;
}


int main(int argc, char *argv[])
{
    int c;
    int i;
    int nproc;
    int proc;
    int rc;
    struct process_state pstate;
    struct thread_state *tstate;
    void *rptr;

    /*
     * default options / arguments
     */

    pstate.blocking = 0;
    pstate.check = 0;
    pstate.inc_bytes = 0;
    pstate.max_bytes = 0;
    pstate.min_bytes = 0;
    pstate.nthread = 1;
    pstate.overlap = 0;
    pstate.reps = 10000;
    pstate.warmup = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    if (nproc == 1) {
        exit(EXIT_SUCCESS);
    }

    while ((c = getopt(argc, argv, "BCOWT:r:h")) != -1) {
        switch (c) {

        case 'B':
            pstate.blocking = 1;
            break;

        case 'C':
            pstate.check = 1;
            break;

        case 'O':
            pstate.overlap = 1;
            break;

        case 'T':
            if ((pstate.nthread = str2size(optarg)) <= 0) {
                usage();
            }
            break;

        case 'W':
            pstate.warmup = 1;
            break;

        case 'r':
            if ((pstate.reps = str2size(optarg)) <= 0) {
                usage();
            }
            break;

        case 'h':
            help();

        default:
            usage();
        }
    }

    if (optind == argc) {
        pstate.min_bytes = 0;
    } else if ((pstate.min_bytes = str2size(argv[optind++])) < 0) {
        usage();
    }

    if (optind == argc) {
        pstate.max_bytes = pstate.min_bytes;
    } else if ((pstate.max_bytes = str2size(argv[optind++])) <
               pstate.min_bytes) {
        usage();
    }

    if (optind == argc) {
        pstate.inc_bytes = 0;
    } else if ((pstate.inc_bytes = str2size(argv[optind++])) < 0) {
        usage();
    }

    if (proc == 0) {
        if (pstate.overlap) {
            printf("mpi-ping: overlapping ping-pong\n");
        } else if (pstate.blocking) {
            printf("mpi-ping: ping-pong (using pstate.blocking send/recv)\n");
        } else {
            printf("mpi-ping: ping-pong\n");
        }
        if (pstate.check) {
            printf("data checking enabled\n");
        }
        printf("nprocs=%d, reps=%d, min bytes=%d, max bytes=%d inc bytes=%d\n",
               nproc, pstate.reps,
               pstate.min_bytes, pstate.max_bytes, pstate.inc_bytes);
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    /*
     * Create threads to do the pings
     */

    tstate = malloc(sizeof(struct thread_state) * pstate.nthread);
    if (!tstate) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < pstate.nthread; i++) {
        tstate[i].rank = i;
        tstate[i].pstate = &pstate;
        rc = pthread_create(&tstate[i].thread, NULL,
                            threadmain, (void *) &tstate[i]);
        if (rc != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    /*
     * Wait for threads to exit
     */
    for (i = 0; i < pstate.nthread; i++) {
        pthread_join(tstate[i].thread, &rptr);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
