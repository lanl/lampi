CC		:= mpicc
CFLAGS		+= -g
CPPFLAGS	+=
LDFLAGS		+=
LDLIBS		+=

all: mpi-ping

clean:
	$(RM) mpi-ping mpi-ping-thread *.o

check: $(TARGET)
	mpirun -np 2 mpi-ping -r 100 0 4M 

mpi-ping-thread: mpi-ping-thread.c
	$(CC) -pthread $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lpthread
