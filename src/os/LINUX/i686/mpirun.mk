ifneq (,$(findstring WITH_ICC, $(CPPFLAGS)))
SRC_MPIRUN += \
	os/LINUX/i686/lock.s
endif
