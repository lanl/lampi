/*
 * A simple MPI collective benchmark using sample based timing.
 */

#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>
#include <getopt.h>


/*
 * collective operation look-up table
 */

enum {
    ALLGATHER = 1 << 0,
    ALLGATHERV = 1 << 1,
    ALLREDUCE = 1 << 2,
    ALLTOALL = 1 << 3,
    ALLTOALLV = 1 << 4,
    ALLTOALLW = 1 << 5,
    BARRIER = 1 << 6,
    BCAST = 1 << 7,
    GATHER = 1 << 8,
    GATHERV = 1 << 9,
    REDUCE = 1 << 10,
    SCAN = 1 << 11,
    SCATTER = 1 << 12,
    SCATTERV = 1 << 13,
    END_MARKER = 1<< 14,
    ALLREDUCE_M = 1 << 15
};

struct {
    char *name;
    int flag;
} op_table[] = {
    { "allgather",	ALLGATHER },
    { "allgatherv",	ALLGATHERV },
    { "allreduce",	ALLREDUCE },     
    { "allreduce_m",	ALLREDUCE_M },     /* MAX instead of SUM */
    { "alltoall",	ALLTOALL },
    { "alltoallv",	ALLTOALLV },
    { "alltoallw",	ALLTOALLW },
    { "barrier",	BARRIER },
    { "bcast",		BCAST },
    { "gather",		GATHER },
    { "gatherv",	GATHERV },
    { "reduce",		REDUCE },
    { "scan",		SCAN },
    { "scatter",	SCATTER },
    { "scatterv",	SCATTERV },
    { NULL,		0 }
};


static void usage(void)
{
    int self;

    MPI_Comm_rank(MPI_COMM_WORLD, &self);

    if (self == 0) {
        fprintf(stderr,
                "Usage: mpi-coll-bench [flags] "
                "<min bytes> [<max bytes>] [<inc bytes>]\n"
                "       mpi-coll-bench -h\n");
    }
    MPI_Finalize();
    exit(EXIT_FAILURE);
}


static void help(void)
{
    int self;

    MPI_Comm_rank(MPI_COMM_WORLD, &self);

    if (self == 0) {
        printf("Usage: mpi-coll-bench [flags] "
               "<min bytes> [<max bytes>] [<inc bytes>]\n"
               "\n"
               "   Flags may be any of\n"
               "      -a                all processes print times\n"
               "      -C                check data\n"
               "      -d                use double instead of int (only for: \n"
	       "                        allreduce\n"
               "      -O <operation>    operation to time\n"
               "      -W                perform warm-up phase\n"
               "      -s number         sample size (repetitions) to time\n"
               "      -n number         number of samples\n"
               "      -h                print this info\n" "\n"
               "   Numbers may be postfixed with 'k', 'M' or 'G'\n"
               "   Operations may be one of allgather, algatherv, etc.\n"
               "   Several operations can be defined simultaneously.\n\n");
    }
    MPI_Finalize();
    exit(EXIT_SUCCESS);
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

        case 'k':
        case 'K':
            return (size << 10);

        case 'm':
        case 'M':
            return (size << 20);

        case 'g':
        case 'G':
            return (size << 30);

        default:
            return (size);
        }

    default:
        return (-1);
    }
}


static double second(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + (double) tv.tv_usec * 1.0e-6;
}


#define SAMPLE(NSAMPLE,SAMPLE_SIZE,CODE) {                         \
    int i, j;                                                      \
    double t, trms, tmin, tmax, tave, ttot;                        \
    tmin = 1.0e99;                                                 \
    trms = tmax = tave = 0.0;                                      \
    for (j = 0; j < NSAMPLE; j++) {                                \
    MPI_Barrier(MPI_COMM_WORLD);                            \
	t = second();	                                           \
	for (i = 0; i < SAMPLE_SIZE; i++) {                        \
	    CODE;                                                  \
        }                                                          \
    MPI_Barrier(MPI_COMM_WORLD);                            \
	t = second() - t;	                                   \
	tmax = t > tmax ? t : tmax;                                \
	tmin = t < tmin ? t : tmin;                                \
	trms += t * t;                                             \
	tave += t;                                                 \
    }                                                              \
    trms /= (double) NSAMPLE;                                      \
    trms = sqrt(trms);                                             \
    tave /= NSAMPLE;                                               \
    trms /= (double) SAMPLE_SIZE;                                  \
    tmax /= (double) SAMPLE_SIZE;                                  \
    tmin /= (double) SAMPLE_SIZE;                                  \
    ttot = tave;                                                   \
    tave /= (double) SAMPLE_SIZE;                                  \
    if (self == 0 || allprint)                                     \
        printf(                                                    \
           "Code:\n"                                               \
           "  %-s\n"                                               \
           "Statistics:\n"                                         \
           "  samples = %d\n"                                      \
           "  sample size = %d\n"                                  \
           "  trms = %e secs\n"                                    \
           "  tmin = %e secs\n"                                    \
           "  tmax = %e secs\n"                                    \
	   "  tave = %e secs\n"                                    \
	   "  ttot = %e secs\n\n",                                 \
           #CODE, NSAMPLE, SAMPLE_SIZE, trms, tmin, tmax, tave, ttot);  \
    if ((self == 0 || allprint) && outFile)                                    \
        fprintf(outFile,                                           \
           "Code:\n"                                               \
           "  %-s\n"                                               \
           "Statistics:\n"                                         \
           "  samples = %d\n"                                      \
           "  sample size = %d\n"                                  \
           "  trms = %e secs\n"                                    \
           "  tmin = %e secs\n"                                    \
           "  tmax = %e secs\n"                                    \
	   "  tave = %e secs\n"                                    \
	   "  ttot = %e secs\n\n",                                 \
           #CODE, NSAMPLE, SAMPLE_SIZE, trms, tmin, tmax, tave, ttot);  \
}

int main(int argc, char *argv[])
{
    int *rbuf;
    int *sbuf;
    double *rbuf_d;
    double *sbuf_d;
    int count;
    int *countv;
    int *dispv;
    int c;
    int i;
    int bytes;
    int nproc;
    int self;
    FILE *outFile = 0;
    char FileName[10];

    /*
     * default options / arguments
     */
    int sample_size = 100;
    int nsample = 10;
    int type_size=sizeof(int);      
    int allprint = 0;
    int check = 0;
    int use_double=0;
    int operations = 0;
    int inc_bytes = 0;
    int max_bytes = 0;
    int min_bytes = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &self);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);


    while ((c = getopt(argc, argv, "adCO:n:s:h")) != -1) {
        switch (c) {

        case 'a':
            allprint = 1;
            break;

        case 'C':
            check = 1;
            break;

        case 'd':
            use_double = 1;
	    type_size=sizeof(double);
            break;

        case 'O':
            for (i = 0; op_table[i].name; i++) {
                if (strcmp(optarg, op_table[i].name) == 0) {
                    operations |= op_table[i].flag;
                    break;
                }
            }
            break;

        case 'n':
            if ((nsample = str2size(optarg)) <= 0) {
                usage();
            }
            break;

        case 's':
            if ((sample_size = str2size(optarg)) <= 0) {
                usage();
            }
            break;

        case 'h':
            help();

        default:
            usage();
        }
    }

    if( self == 0 ) {
      sprintf(&FileName[0],"coll_out\0");
      outFile=fopen(FileName,"wb");
    }
    if (operations == 0) {
        if (self == 0) {
            fprintf(stderr, "No operations defined\n");
        }
        usage();
    }

    if (optind == argc) {
        min_bytes = 0;
    } else if ((min_bytes = str2size(argv[optind++])) < 0) {
        usage();
    }

    if (optind == argc) {
        max_bytes = min_bytes;
    } else if ((max_bytes = str2size(argv[optind++])) < min_bytes) {
        usage();
    }

    if (optind == argc) {
        inc_bytes = 0;
    } else if ((inc_bytes = str2size(argv[optind++])) < 0) {
        usage();
    }

    if (nproc == 1) {
        MPI_Finalize();
        exit(EXIT_SUCCESS);
    }

    if (operations & GATHER     ||
        operations & GATHERV    ||
        operations & ALLGATHER  ||
        operations & ALLGATHERV ||
        operations & ALLTOALL   ||
        operations & ALLTOALLV) {
        if ((rbuf = (int *) malloc(nproc * max_bytes ? nproc * max_bytes
                                   : 8)) == NULL) {
            perror("malloc");
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
    }
    else {
        if ((rbuf = (int *) malloc(max_bytes ? max_bytes : 8)) == NULL) {
            perror("malloc");
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
    }

    if (operations & SCATTER || operations & SCATTERV) {
        if ((sbuf = (int *) malloc(nproc * max_bytes ? nproc * max_bytes
                                   : 8)) == NULL) {
            perror("malloc");
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
    }
    else {
        if ((sbuf = (int *) malloc(max_bytes ? max_bytes : 8)) == NULL) {
            perror("malloc");
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
    }
    sbuf_d = (void *) sbuf;
    rbuf_d = (void *) rbuf;

    if ((countv = (int *) malloc(sizeof(int) * nproc)) == NULL) {
        perror("malloc");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    if ((dispv = (int *) malloc(sizeof(int) * nproc)) == NULL) {
        perror("malloc");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    /* use non-zero data for MPI_SUM, MPI_MAX */
    count = max_bytes/type_size;
    if (use_double) {
      for (i=0; i<count; ++i) sbuf_d[i]=1.01;
    }else{
      for (i=0; i<count; ++i) sbuf[i]=1;
    }

    if (check) {
        for (i = 0; i < max_bytes; i++) {
            rbuf[i] = i & 255;
            rbuf[i] = 0;
        }
    }

    if (self == 0) {
        printf("--------------------------------"
               "--------------------------------\n"
               "MPI collective benchmark\n"
               "--------------------------------"
               "--------------------------------\n");
        printf("Number of processes: %d\n", nproc);
        printf("Number of samples:   %d\n", nsample);
        printf("Sample size:         %d\n", sample_size);
        printf("Minimum bytes:       %d\n", min_bytes);
        printf("Maximum bytes:       %d\n", max_bytes);
        printf("Inc. bytes:          %d\n", inc_bytes);
        if (check) {
            printf("Data checking enabled\n");
        }
        printf("Operations:\n");
        for (i = 0; op_table[i].name; i++) {
            if (operations & op_table[i].flag) {
                printf("  %s", op_table[i].name);
            }
        }
        printf("\n\n");
        fflush(stdout);
    }

    /*
     * Main loop
     */

    for (bytes = min_bytes; bytes <= max_bytes;
         bytes = inc_bytes ? bytes + inc_bytes : bytes ? 2 * bytes : 1) {

        count = bytes / type_size;
        for (i = 0; i < nproc; i++) {
            countv[i] = count;
        }
        for (i = 0; i < nproc; i++) {
            dispv[i] = i * count;
        }
        if (self == 0) {
            printf("--------------------------------"
                   "--------------------------------\n"
                   "Bytes = %d    count=%d\n"
                   "--------------------------------"
                   "--------------------------------\n",
                   bytes,count);
        }

        if (operations & ALLGATHER) {
            SAMPLE(nsample, sample_size,
                   MPI_Allgather(sbuf, count, MPI_INT,
                                 rbuf, count, MPI_INT,
                                 MPI_COMM_WORLD);
                );
        }

        if (operations & ALLGATHERV) {
            SAMPLE(nsample, sample_size,
                   MPI_Allgatherv(sbuf, count, MPI_INT,
                                  rbuf, countv, dispv, MPI_INT,
                                  MPI_COMM_WORLD);
                );
        }

        if (operations & ALLREDUCE) {
	  if (use_double) {
	    double testbuf[count]; 
/* 
use testbuf instead of sbuf_d - code will hang on QSC
because of client segfault on non initialized floating point error?
*/

            SAMPLE(nsample, sample_size,
                   MPI_Allreduce(sbuf_d, rbuf_d, count, MPI_DOUBLE, MPI_SUM,
                                 MPI_COMM_WORLD);
		   );
          }else{
            SAMPLE(nsample, sample_size,
                   MPI_Allreduce(sbuf, rbuf, count, MPI_INT, MPI_SUM,
                                 MPI_COMM_WORLD);
                );
	  }
        }

        if (operations & ALLREDUCE_M) {
	  if (use_double){
            SAMPLE(nsample, sample_size,
                   MPI_Allreduce(sbuf_d, rbuf_d, count, MPI_DOUBLE, MPI_MAX,
                                 MPI_COMM_WORLD);
		   );
	  }else{
	    SAMPLE(nsample, sample_size,
		   MPI_Allreduce(sbuf, rbuf, count, MPI_INT, MPI_MAX,
				 MPI_COMM_WORLD);
		   );
	  }
        }


        if (operations & ALLTOALL) {
            SAMPLE(nsample, sample_size,
                   MPI_Alltoall(sbuf, count, MPI_INT,
                                rbuf, count, MPI_INT,
                                MPI_COMM_WORLD);
                );
        }

        if (operations & ALLTOALLV) {
            SAMPLE(nsample, sample_size,
                   MPI_Alltoallv(sbuf, countv, dispv, MPI_INT,
                                 rbuf, countv, dispv, MPI_INT,
                                 MPI_COMM_WORLD);
                );
        }

        if (operations & ALLTOALLW) {
            SAMPLE(nsample, sample_size,
                   /* MPI_Alltoallw(); */
                   );
        }

        if (operations & BARRIER) {
            SAMPLE(nsample, sample_size,
                   MPI_Barrier(MPI_COMM_WORLD);
                );
        }

        if (operations & BCAST) {
            SAMPLE(nsample, sample_size,
                   MPI_Bcast(rbuf, count, MPI_INT,
                             0, MPI_COMM_WORLD);
                );
        }

        if (operations & GATHER) {
            SAMPLE(nsample, sample_size,
                   MPI_Gather(sbuf, count, MPI_INT,
                              rbuf, count, MPI_INT,
                              0, MPI_COMM_WORLD);
                );
        }

        if (operations & GATHERV) {
            SAMPLE(nsample, sample_size,
                   MPI_Gatherv(sbuf, count, MPI_INT,
                               rbuf, countv, dispv, MPI_INT,
                               0, MPI_COMM_WORLD);
                );
        }

        if (operations & REDUCE) {
            SAMPLE(nsample, sample_size,
                   MPI_Reduce(sbuf, rbuf, count, MPI_INT, MPI_SUM,
                              i%nproc, MPI_COMM_WORLD);
                );
        }

        if (operations & SCAN) {
            SAMPLE(nsample, sample_size,
                   MPI_Scan(sbuf, rbuf, count, MPI_INT, MPI_SUM,
                            MPI_COMM_WORLD);
                );
        }

        if (operations & SCATTER) {
            SAMPLE(nsample, sample_size,
                   MPI_Scatter(sbuf, count, MPI_INT,
                               rbuf, count, MPI_INT,
                               0, MPI_COMM_WORLD);
                );
        }

        if (operations & SCATTERV) {
            SAMPLE(nsample, sample_size,
                   MPI_Scatterv(sbuf, countv, dispv, MPI_INT,
                                rbuf, count, MPI_INT,
                                0, MPI_COMM_WORLD);
                );
        }

    }

    if (self == 0) {
        printf("All done\n");
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}
