CC		:= mpicc
CFLAGS		+= -g
CPPFLAGS	+=
LDFLAGS		+=
LDLIBS		+=

all: hello mpi-ping mpi-coll-bench

clean:
	$(RM) hello mpi-coll-bench mpi-ping mpi-ping-thread *.o

mpi-ping-thread: mpi-ping-thread.c
	$(CC) -pthread $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lpthread
