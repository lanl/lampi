CFLAGS          += $(MPI_COMPILE_FLAGS)
LDFLAGS         += $(MPI_LD_FLAGS)
LDLIBS          += -lmpi
TARGET          := mpi-ping mpi-ping-thread
MPIRUN          := $(MPI_ROOT)/bin/mpirun

all: $(TARGET)

clean:
	$(RM) $(TARGET)

check: $(TARGET)
	$(MPIRUN) -np 2 -exe mpi-ping -r 100 0 4M 

mpi-ping-thread: mpi-ping-thread.c
	$(CC) -pthread $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lpthread
