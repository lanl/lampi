ifneq (,$(findstring WITH_ECC, $(CPPFLAGS)))
SRC_LIBMPI += \
	os/LINUX/ia64/lock.s
endif
