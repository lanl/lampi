SRC_LIBMPI += \
	src/init/environ.cc \
	src/init/fork_many.cc \
	src/init/init.cc \
	src/init/init_debug.cc \
	src/init/init_proc.cc \
	src/init/state.cc \
	src/init/stub.cc

ifneq (,$(findstring ENABLE_SHARED_MEMORY, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_shared_memory.cc
endif

ifneq (,$(findstring ENABLE_UDP, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_udp.cc
endif

ifneq (,$(findstring ENABLE_TCP, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_tcp.cc
endif

ifneq (,$(findstring ENABLE_QSNET, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_quadrics.cc src/init/init_rms.cc
endif

ifneq (,$(findstring ENABLE_GM, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_gm.cc
endif

ifneq (,$(findstring ENABLE_LSF, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_lsf.cc
endif

ifneq (,$(findstring ENABLE_INFINIBAND, $(CPPFLAGS)))
SRC_LIBMPI += src/init/init_ib.cc
endif
