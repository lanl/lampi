CFLAGS  += $(MPI_COMPILE_FLAGS)
LDFLAGS += $(MPI_LD_FLAGS)
#LDFLAGS += -Wl,-rpath $(MPI_ROOT)/lib
LDLIBS  := -lmpi -lm

TARGET  := mpi-coll-bench
MPIRUN  := $(MPI_ROOT)/bin/mpirun

all: $(TARGET)

clean:
	$(RM) $(TARGET)

check: $(TARGET)
	$(MPIRUN) -np 4 -exe mpi-coll-bench \
        -O allgather \
        -O allreduce \
        -O barrier \
        -O bcast \
        -s 10 \
        -n 10 \
        0 512k 
