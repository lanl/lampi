CC		:= mpicc
CFLAGS		+= -g
CPPFLAGS	+=
LDFLAGS		+=
LDLIBS		+=

all: mpi-hello mpi-ping mpi-coll-bench

clean:
	$(RM) mpi-hello mpi-coll-bench mpi-ping mpi-ping-thread *.o lampi.log

mpi-ping-thread: mpi-ping-thread.c
	$(CC) -pthread $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lpthread
