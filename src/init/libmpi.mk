SRC_LIBMPI += \
	init/environ.cc \
	init/fork_many.cc \
	init/init.cc \
	init/init_debug.cc \
	init/init_proc.cc \
	init/state.cc \
	init/stub.cc

ifneq (,$(findstring SHARED_MEMORY, $(CPPFLAGS)))
SRC_LIBMPI += init/init_shared_memory.cc init/init_local_coll.cc
endif

ifneq (,$(findstring UDP, $(CPPFLAGS)))
SRC_LIBMPI += init/init_udp.cc
endif

ifneq (,$(findstring QUADRICS, $(CPPFLAGS)))
SRC_LIBMPI += init/init_quadrics.cc init/init_rms.cc
endif

ifneq (,$(findstring GM, $(CPPFLAGS)))
SRC_LIBMPI += init/init_gm.cc
endif

ifneq (,$(findstring LSF, $(CPPFLAGS)))
SRC_LIBMPI += init/init_lsf.cc
endif
