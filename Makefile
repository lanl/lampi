######################################################################
#
# Top-level makefile for LA-MPI
#
# This make file sets up build directories using $(LNDIR), then does the
# real make in the build directory.
#
######################################################################

# general definitions

.PHONY: all build build-dir debug debug-dir debug-install clean distclean install lampi lampi-debug \
romio-clean romio-distclean combined.so combined-debug.so combined.a combined-debug.a romio-dir romio-debug-dir

.PHONY: all clean distclean install build debug

SHELL		:= /bin/sh

######################################################################

# configuration selection is based on the variable CONFIG

CONFIG := $(shell src/conf/config.guess)

ifeq ($(findstring darwin, $(CONFIG)), darwin)
LNDIR		:= /usr/X11R6/bin/lndir
NJOBS		:=
else
LNDIR		:= /usr/bin/X11/lndir
NJOBS		:= -j 32
endif
BUILD_DIR	:= build-$(CONFIG)
DEBUG_DIR	:= debug-$(CONFIG)

include romio-conf/$(CONFIG).mk

######################################################################

all: build

clean: lampi-clean romio-clean

distclean:
	$(RM) -r build-* debug-*

install: build-install

######################################################################

build: lampi-build romio-build combined.so combined.a

build-dir:
	@([ -d $(BUILD_DIR) ] || mkdir $(BUILD_DIR)) ; $(LNDIR) ../src $(BUILD_DIR)

build-install: build
	(cd $(BUILD_DIR); $(MAKE) install)

debug: lampi-debug romio-debug combined-debug.so combined-debug.a

debug-dir: 
	@([ -d $(DEBUG_DIR) ] || mkdir $(DEBUG_DIR)) ; $(LNDIR) ../src $(DEBUG_DIR)

debug-install: debug
	(cd $(DEBUG_DIR); $(MAKE) install)

lampi: lampi-build

lampi-build: build-dir 
	(cd $(BUILD_DIR); $(MAKE) $(NJOBS) CONFIG=$(CONFIG) all)

lampi-install: build-dir
	(cd $(BUILD_DIR); $(MAKE) $(NJOBS) CONFIG=$(CONFIG) all; $(MAKE) install)

lampi-debug: debug-dir
	(cd $(DEBUG_DIR); $(MAKE) $(NJOBS) CONFIG=$(CONFIG) ULMDEBUG=0 all)

lampi-debug-install: debug-dir
	(cd $(DEBUG_DIR); $(MAKE) $(NJOBS) CONFIG=$(CONFIG) ULMDEBUG=0 all; ULM_DBG=1; $(MAKE) install)

lampi-clean: romio-clean
	-[ -d $(BUILD_DIR) ] && ($(LNDIR) ../src $(BUILD_DIR) ; cd $(BUILD_DIR) ; $(MAKE) clean)
	-[ -d $(DEBUG_DIR) ] && ($(LNDIR) ../src $(DEBUG_DIR) ; cd $(DEBUG_DIR) ; $(MAKE) clean)

romio-clean:
	-[ -d $(BUILD_DIR)/romio ] && (cd $(BUILD_DIR)/romio ; $(MAKE) clean)
	-[ -d $(DEBUG_DIR)/romio ] && (cd $(DEBUG_DIR)/romio ; $(MAKE) clean)

romio-distclean:
	$(RM) -r build-*/romio debug-*/romio

romio-build: romio-dir
	@if [ -f $(BUILD_DIR)/romio/Makefile ] ; then \
	  echo WARNING: Using previously built ROMIO; \
        else \
          (cd $(BUILD_DIR)/romio ; ./configure $(ROMIOCONFIG) $(ROMIOCONFIG_F90) -mpi=lampi -mpiolib=libmpio.a -mpiincdir=../include -mpilib=$(PWD)/$(BUILD_DIR)/libmpi.so -make="$(MAKE)" >configure.out ; $(MAKE) $(NJOBS)) \
	fi

romio-debug: romio-debug-dir
	@if [ -f $(DEBUG_DIR)/romio/Makefile ] ; then \
	  echo WARNING: Using previously built ROMIO; \
        else \
          (cd $(DEBUG_DIR)/romio ; ./configure $(ROMIOCONFIG_DEBUG) $(ROMIOCONFIG_F90) -mpi=lampi -mpiolib=libmpio.a -mpiincdir=../include -mpilib=$(PWD)/$(DEBUG_DIR)/libmpi.so -make="$(MAKE)" >configure.out ; $(MAKE) $(NJOBS)) \
	fi

romio-dir: build-dir lampi-build
	@[ -d $(BUILD_DIR)/romio ] || (mkdir $(BUILD_DIR)/romio ; $(LNDIR) ../../romio-1.2.4.1 $(BUILD_DIR)/romio)

romio-debug-dir: debug-dir lampi-debug
	@[ -d $(DEBUG_DIR)/romio ] || (mkdir $(DEBUG_DIR)/romio ; $(LNDIR) ../../romio-1.2.4.1 $(DEBUG_DIR)/romio)

combined.so: lampi-build romio-build
	@(cd $(BUILD_DIR) ; $(MAKE) CONFIG=$(CONFIG) MPIO_OBJECTS="$(ROMIOCONFIG_LINKLIB)" MPIO_LIB=romio/libmpio.a libmpiandio.so)

combined.a: lampi-build romio-build
	@(cd $(BUILD_DIR) ; $(MAKE) CONFIG=$(CONFIG) MPIO_OBJECTS="`echo romio/objects/*`" MPIO_LIB=romio/libmpio.a libmpiandio.a)

combined-debug.so: lampi-debug romio-debug
	@(cd $(DEBUG_DIR) ; $(MAKE) CONFIG=$(CONFIG) MPIO_OBJECTS="$(ROMIOCONFIG_LINKLIB)" MPIO_LIB=romio/libmpio.a libmpiandio.so)

combined-debug.a: lampi-debug romio-debug
	@(cd $(DEBUG_DIR) ; $(MAKE) CONFIG=$(CONFIG) MPIO_OBJECTS="`echo romio/objects/*`" MPIO_LIB=romio/libmpio.a libmpiandio.a)
