######################################################################
#
# Makefile for LA-MPI
#
# This make file is patterned after the suggestions in "Recursive Make
# Considered Harmful" by Peter Miller:
#	http://www.pcug.org.au/~millerp/rmch/recu-make-cons-harm.html
#
######################################################################

# general definitions

.PHONY:		all clean distclean install
SHELL		:= /bin/sh
INSTALL		:= $(shell pwd)/install-sh -c
VERSION	:= include/internal/version.h

######################################################################
#
# Common definitions for all platforms
#
######################################################################

CPPFLAGS       := -DHAVE_PRAGMA_WEAK

######################################################################

# configuration selection is based on the variable CONFIG

CONFIG := $(shell conf/config.guess)

include conf/$(CONFIG).mk

TARGET		:=
TARGET		+= libmpi.a
TARGET		+= libmpi.$(SOSUFFIX)
TARGET		+= mpirun

######################################################################

# modules (subdirectories) for each target

MODULES_LIBMPI	:= $(MODULES_OS)
MODULES_LIBMPI	+= client
MODULES_LIBMPI	+= interface
MODULES_LIBMPI	+= mpi/c
MODULES_LIBMPI	+= mpi/c2f
MODULES_LIBMPI	+= mpi/f
MODULES_LIBMPI	+= mpi/internal
MODULES_LIBMPI	+= queue
MODULES_LIBMPI	+= util
MODULES_LIBMPI  += collective
MODULES_LIBMPI  += collective/p2p
MODULES_LIBMPI  += ctnetwork
MODULES_LIBMPI  += init
MODULES_LIBMPI  += mem
MODULES_LIBMPI  += path/common
MODULES_LIBMPI  += threads

# include shared memory support if SHARED_MEMORY is defined
# -- this is currently mandatory
ifneq (,$(findstring SHARED_MEMORY, $(CPPFLAGS)))
MODULES_LIBMPI	+= path/sharedmem
endif

# include multicast support if SHARED_MEMORY is defined
ifneq (,$(findstring SHARED_MEMORY, $(CPPFLAGS)))
MODULES_LIBMPI	+= path/mcast
endif

# include UDP/IP support if UDP is defined
ifneq (,$(findstring UDP, $(CPPFLAGS)))
MODULES_LIBMPI	+= path/udp
endif

# include Quadrics Elan3 support if QUADRICS is defined
ifneq (,$(findstring QUADRICS, $(CPPFLAGS)))
MODULES_LIBMPI	+= path/quadrics
MODULES_LIBMPI  += collective/quadrics
endif

# include GM if GM is defined
ifneq (,$(findstring -DGM, $(CPPFLAGS)))
MODULES_LIBMPI	+= path/gm
endif

ifdef USE_CT
CPPFLAGS	+= -DUSE_CT=1
endif

ifdef ULM_DBG
CPPFLAGS	+= -DULM_DBG=1
endif

MODULES_MPIRUN	:= $(MODULES_OS)
MODULES_MPIRUN	+= client
MODULES_MPIRUN	+= init
MODULES_MPIRUN	+= interface
MODULES_MPIRUN	+= run
MODULES_MPIRUN	+= util
MODULES_MPIRUN  += ctnetwork
MODULES_MPIRUN  += threads

######################################################################

# GNU conforming installation directory variable names.
# Over-ride with command line arguments, e.g.
#	make prefix=/opt libdir=/opt/lib64 install

;prefix = /usr/local
;prefix = $(HOME)/usr/$(CONFIG)
prefix = $(PWD)/dist-$(CONFIG)
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
includedir = $(prefix)/include
infodir = $(prefix)/info
mandir = $(prefix)/man
etcdir = $(prefix)/etc

######################################################################

# determine the source files by including the description of each module

SRC_LIBMPI	:=
include $(patsubst %,%/libmpi.mk,$(MODULES_LIBMPI))

SRC_MPIRUN	:=
include $(patsubst %,%/mpirun.mk,$(MODULES_MPIRUN))

# determine the object files

OBJ_LIBMPI	:=
OBJ_LIBMPI	+= $(patsubst %.c,%.o,$(filter %.c,$(SRC_LIBMPI)))
OBJ_LIBMPI	+= $(patsubst %.cc,%.o,$(filter %.cc,$(SRC_LIBMPI)))
OBJ_LIBMPI	+= $(patsubst %.s,%.o,$(filter %.s,$(SRC_LIBMPI)))

OBJ_MPIRUN	:=
OBJ_MPIRUN	+= $(patsubst %.c,%.o,$(filter %.c,$(SRC_MPIRUN)))
OBJ_MPIRUN	+= $(patsubst %.cc,%.o,$(filter %.cc,$(SRC_MPIRUN)))
OBJ_MPIRUN	+= $(patsubst %.s,%.o,$(filter %.s,$(SRC_MPIRUN)))

# determine dependency files

DEPENDENCIES	:=
DEPENDENCIES	+= $(patsubst %.c,%.d,$(filter %.c,$(SRC_LIBMPI)))
DEPENDENCIES	+= $(patsubst %.cc,%.d,$(filter %.cc,$(SRC_LIBMPI)))
DEPENDENCIES	+= $(patsubst %.c,%.d,$(filter %.c,$(SRC_MPIRUN)))
DEPENDENCIES	+= $(patsubst %.cc,%.d,$(filter %.cc,$(SRC_MPIRUN)))

# target rules

all: $(VERSION) $(TARGET)

clean:
	$(RM) $(OBJ_LIBMPI) $(DEPENDENCIES) $(OBJ_MPIRUN)

distclean: clean
	$(RM) $(TARGET) $(VERSION)

install: all
	umask 002 ; \
	$(INSTALL) -m 775 -d $(prefix) ; \
	$(INSTALL) -m 775 -d $(bindir) ; \
	$(INSTALL) -m 775 -d $(includedir) ; \
	$(INSTALL) -m 775 -d $(libdir) ; \
	$(INSTALL) -m 775 -d $(etcdir) ; \
	$(INSTALL) -m 775 -d $(includedir)/mpi ; \
	$(INSTALL) -m 775 -d $(includedir)/ulm ; \
	$(INSTALL) -m 775 -d $(prefix)/share ; \
	$(INSTALL) -m 775 -d $(prefix)/share/man ; \
	$(INSTALL) -m 775 -d $(prefix)/share/man/man1 ; \
	$(INSTALL) -m 775 -d $(prefix)/share/lampi ; \
	$(INSTALL) -m 775 libmpi.$(SOSUFFIX) $(libdir)/libmpi.$(SOSUFFIX) ; \
	$(INSTALL) -m 775 libmpiandio.$(SOSUFFIX) $(libdir)/libmpi.$(SOSUFFIX) ; \
	$(INSTALL) -m 664 libmpiandio.a $(libdir)/libmpi.a ; \
	$(INSTALL) -m 775 mpirun $(bindir) ; \
	$(INSTALL) -m 664 ../man/lampi.man1 $(prefix)/share/man/man1/lampi.1 ; \
	$(INSTALL) -m 664 ../man/mpirun.man1 $(prefix)/share/man/man1/mpirun.1 ; \
	$(INSTALL) -m 664 ../RELEASE_NOTES $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 ../man/lampi.html $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 ../man/mpirun.html $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 ../etc/lampi.conf $(etcdir)/lampi.conf ; \
	(cd $(prefix) ; ln -s share/man .) ; \
	(cd ../src/include ; for f in *.h; do $(INSTALL) -m 664 $$f $(includedir); done) ; \
	(cd romio/include ; for f in *.h; do $(INSTALL) -m 664 $$f $(includedir); done) ; \
	(cd ../src/include/mpi ; for f in *.h; do $(INSTALL) -m 664 $$f $(includedir)/mpi; done) ; \
	(cd ../src/include/ulm ; for f in *.h; do $(INSTALL) -m 664 $$f $(includedir)/ulm; done) ; \
	if [ -f $(includedir)/mpio.h ] ; then \
		mv $(includedir)/mpiandio.h $(includedir)/mpi.h; \
	fi

$(VERSION): VERSION
	echo '#define LAMPI_VERSION "'`cat VERSION`'"' > $@

mpirun: $(VERSION) $(OBJ_MPIRUN)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_MPIRUN) -o $@ $(OBJ_MPIRUN) $(LDLIBS) $(LDLIBS_MPIRUN)

libmpi.a: $(VERSION) $(OBJ_LIBMPI)
	@for f in $(OBJ_LIBMPI); do l=`echo $$f | tr '/' '.'`; ln -sf $$f src.$$l; done
	$(AR) $(ARFLAGS) $@ src.*.o
	$(RANLIB) $@
	@$(RM) src.*.o

libmpi.$(SOSUFFIX): $(VERSION) $(OBJ_LIBMPI)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_LIBMPI) -o $@ $(OBJ_LIBMPI) $(LDLIBS) $(LDLIBS_LIBMPI)

libmpiandio.$(SOSUFFIX): $(VERSION) $(OBJ_LIBMPI) $(MPIO_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_LIBMPI) \
		-o $@ $(OBJ_LIBMPI) $(MPIO_OBJECTS) $(LDLIBS) $(LDLIBS_LIBMPI)

libmpiandio.a: $(VERSION) $(OBJ_LIBMPI) $(MPIO_LIB)
	@for f in $(OBJ_LIBMPI); do l=`echo $$f | tr '/' '.'`; ln -sf $$f src.$$l; done
	$(AR) $(ARFLAGS) $@ src.*.o
	@$(RM) src.*.o
	$(AR) $(ARFLAGS) $@ $(MPIO_OBJECTS)
	$(RANLIB) $@

# include dependency files, if we were not invoked as "make clean"
# or "make distclean"

ifndef NODEPS
ifneq ($(findstring clean, $(MAKECMDGOALS)),clean)
-include $(DEPENDENCIES)
endif
endif

# rules for calculating dependency files

%.d: %.c $(VERSION)
	$(CC) -M $(CPPFLAGS) $< | sed  -e 's@^.*\.o:@$*.o $*.d:@' > $@

%.d: %.cc $(VERSION)
	$(CXX) -M $(CPPFLAGS) $< | sed  -e 's@^.*\.o:@$*.o $*.d:@' > $@
