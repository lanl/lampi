ifdef MPI_ROOT
MPI_INC		?= $(MPI_ROOT)/include
MPI_LIB		?= $(MPI_ROOT)/lib
else
MPI_INC		?= ../..
MPI_LIB		?= ../..
endif

CPPFLAGS        := -I$(MPI_INC)
LDFLAGS         := -L$(MPI_LIB)
LDLIBS          := -lmpi

TARGET          := mpi-ping mpi-ping-thread
MPIRUN          := ../../mpirun

all: $(TARGET)

clean:
	$(RM) $(TARGET)

check: $(TARGET)
	$(MPIRUN) -np 2 -exe mpi-ping -r 100 0 4M 


mpi-ping-thread: mpi-ping-thread.c
	$(CC) -pthread $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS) -lpthread
