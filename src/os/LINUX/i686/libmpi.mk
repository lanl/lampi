ifneq (,$(findstring WITH_ICC, $(CPPFLAGS)))
SRC_LIBMPI += \
	os/LINUX/i686/lock.s
endif
