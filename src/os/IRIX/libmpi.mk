SRC_LIBMPI += \
	src/os/IRIX/lock.s \
	src/os/IRIX/atomic.cc

ifeq "$(enable_numa)" "yes"
SRC_LIBMPI += \
	src/os/IRIX/PlatformBarrierSetup.cc
endif
