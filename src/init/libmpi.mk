SRC_LIBMPI += \
	src/init/environ.cc \
	src/init/fork_many.cc \
	src/init/init.cc \
	src/init/init_debug.cc \
	src/init/init_proc.cc \
	src/init/state.cc \
	src/init/stub.cc \
	src/init/tty.cc

ifeq "$(enable_shared_memory)" "yes"
SRC_LIBMPI += src/init/init_shared_memory.cc
endif

ifeq "$(enable_udp)" "yes"
SRC_LIBMPI += src/init/init_udp.cc
endif

ifeq "$(enable_tcp)" "yes"
SRC_LIBMPI += src/init/init_tcp.cc
endif

ifeq "$(enable_qsnet)" "yes"
SRC_LIBMPI += src/init/init_quadrics.cc
endif

ifeq "$(enable_rms)" "yes"
SRC_LIBMPI += src/init/init_rms.cc
endif

ifeq "$(enable_gm)" "yes"
SRC_LIBMPI += src/init/init_gm.cc
endif

ifeq "$(enable_lsf)" "yes"
SRC_LIBMPI += src/init/init_lsf.cc
endif

ifeq "$(enable_ib)" "yes"
SRC_LIBMPI += src/init/init_ib.cc
endif
