CONFIG		:= $(shell ../../conf/config.guess)

include ../../conf/$(CONFIG).mk

CPPFLAGS        := -I../../include
LDFLAGS         := -L../..
LDLIBS          := -lmpi -lm

ifneq (,$(findstring osf, $(CONFIG)))
LDFLAGS         += -rpath ../..
endif

TARGET          := mpi-coll-bench
MPIRUN          := ../../mpirun

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
