CONFIG		:= $(shell ../../conf/config.guess)

include ../../conf/$(CONFIG).mk

CPPFLAGS        := -I../../include
LDFLAGS         := -L../..
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
