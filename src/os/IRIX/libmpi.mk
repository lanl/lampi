SRC_LIBMPI += \
	os/IRIX/lock.s \
	os/IRIX/atomic.cc

ifeq (,$(findstring NUMA, $(CPPFLAGS)))
SRC_LIBMPI += \
	os/IRIX/PlatformBarrierSetup.cc
endif
