SRC_LIBMPI += \
	src/os/IRIX/lock.s \
	src/os/IRIX/atomic.cc

ifeq (,$(findstring NUMA, $(CPPFLAGS)))
SRC_LIBMPI += \
	src/os/IRIX/PlatformBarrierSetup.cc
endif
