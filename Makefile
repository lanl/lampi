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

# special strings for text manipulation
comma		:=,

######################################################################

# GNU / autoconf conforming variable names

srcdir		:= .
top_srcdir	:= .
VPATH		:= .
prefix		:= /users/taylorm/liblampi2
exec_prefix	:= ${prefix}
bindir		:= ${exec_prefix}/bin
sbindir		:= ${exec_prefix}/sbin
libexecdir	:= ${exec_prefix}/libexec
datadir		:= ${prefix}/share
sysconfdir	:= ${prefix}/etc
sharedstatedir	:= ${prefix}/com
localstatedir	:= ${prefix}/var
libdir		:= ${exec_prefix}/lib
infodir		:= ${prefix}/info
mandir		:= ${prefix}/man
includedir	:= ${prefix}/include
top_builddir	:= .
INSTALL		:= ./conf/install-sh -c

######################################################################

CC		:= cc
CXX		:= cxx
CFLAGS		:= -arch host -accept restrict_keyword -g3 -O3
CXXFLAGS	:= -arch host -accept restrict_keyword -g3 -O3 -model ansi
# added -Isrc/path/quadrics
CPPFLAGS	:= -DHAVE_CONFIG_H  -pthread -w -DNDEBUG -DHAVE_PRAGMA_WEAK -DENABLE_RMS -Isrc/path/quadrics -I/usr/opt/rms/include -DENABLE_UDP -DENABLE_QSNET -DENABLE_SHARED_MEMORY -DENABLE_RELIABILITY -DENABLE_CHECK_API_ARGS
LDFLAGS		:=  -L/usr/opt/rms/lib
LDLIBS		:=  -lrt -lrmscall -lelan -lelan3
RANLIB		:= ranlib
LN_S		:= ln -s
FC		:= f77
FFLAGS		:= -g
F90		:= 

LDFLAGS_LIBMPI	:=  -shared -all $(patsubst %,-rpath %,$(rpath)) -none
LDFLAGS_MPIRUN	:= 
LDLIBS_LIBMPI	:= 
LDLIBS_MPIRUN	:= 

EXEEXT		:= 
OBJEXT		:= o
LIBEXT		:= a
DLLEXT		:= so

######################################################################

# Optional feature/package autoconf substitutions

enable_debug	:= 
enable_dynamic	:= yes
enable_static	:= yes
with_romio	:= no

ifeq "$(with_romio)" "yes"
ROMIODIR	:= romio-1.2.4.1
ROMIOLIB	:= $(PWD)/$(ROMIODIR)/objects/romio.$(LIBEXT)
OBJ_ROMIO	:= $(ROMIODIR)/objects/*.$(OBJEXT)
ROMIOCONFIGURE  := $(srcdir)/$(ROMIODIR)/configure
ROMIOCONFIGURE	+= -noprofile -cc=$(CC) -fc=$(FC) -f90=$(F90)
ifeq "$(enable_debug)" "yes"
ROMIOCONFIGURE	+= -debug
endif
ROMIOCONFIGURE	+= -mpi=lampi
ROMIOCONFIGURE	+= -mpiolib=$(ROMIOLIB)
ROMIOCONFIGURE	+= -mpiincdir=$(srcdir)/src/include
ROMIOCONFIGURE	+= -mpilib=$(PWD)/lampi.$(DLLEXT)
ROMIOCONFIGURE	+= -make=$(MAKE) 
endif

######################################################################

# modules (subdirectories) for each target

MODULES_LIBMPI	:=  src/client src/collective src/collective/p2p src/ctnetwork src/init src/interface src/mem src/mpi/c src/mpi/c2f src/mpi/f src/mpi/internal src/path/common src/queue src/threads src/util src/path/udp src/path/quadrics src/collective/quadrics src/path/sharedmem  src/os src/os/TRU64
MODULES_MPIRUN	:=  src/client src/ctnetwork src/init src/interface src/popt src/run src/threads src/util  src/os src/os/TRU64

######################################################################

# final flags needed by all platforms

CPPFLAGS	+= -I. -I$(srcdir)/src -I$(srcdir)/src/include
LDLIBS		+= -lpthread

######################################################################

# determine the source files by including the description of each module

SRC_LIBMPI	:=
include $(patsubst %,$(srcdir)/%/libmpi.mk,$(MODULES_LIBMPI))

SRC_MPIRUN	:=
include $(patsubst %,$(srcdir)/%/mpirun.mk,$(MODULES_MPIRUN))

# determine the object files

OBJ_LIBMPI	:=
OBJ_LIBMPI	+= $(patsubst %.c,%.$(OBJEXT),$(filter %.c,$(SRC_LIBMPI)))
OBJ_LIBMPI	+= $(patsubst %.cc,%.$(OBJEXT),$(filter %.cc,$(SRC_LIBMPI)))
OBJ_LIBMPI	+= $(patsubst %.s,%.$(OBJEXT),$(filter %.s,$(SRC_LIBMPI)))

OBJ_MPIRUN	:=
OBJ_MPIRUN	+= $(patsubst %.c,%.$(OBJEXT),$(filter %.c,$(SRC_MPIRUN)))
OBJ_MPIRUN	+= $(patsubst %.cc,%.$(OBJEXT),$(filter %.cc,$(SRC_MPIRUN)))
OBJ_MPIRUN	+= $(patsubst %.s,%.$(OBJEXT),$(filter %.s,$(SRC_MPIRUN)))

# determine dependency files

DEPENDENCIES	:=
DEPENDENCIES	+= $(patsubst %.c,%.d,$(filter %.c,$(SRC_LIBMPI)))
DEPENDENCIES	+= $(patsubst %.cc,%.d,$(filter %.cc,$(SRC_LIBMPI)))
DEPENDENCIES	+= $(patsubst %.c,%.d,$(filter %.c,$(SRC_MPIRUN)))
DEPENDENCIES	+= $(patsubst %.cc,%.d,$(filter %.cc,$(SRC_MPIRUN)))

######################################################################

# targets

TARGET		:= mpirun$(EXEEXT)
ifeq "$(enable_dynamic)" "yes"
TARGET		+= libmpi.$(DLLEXT)
endif
ifeq "$(enable_static)" "yes"
TARGET		+= libmpi.$(LIBEXT)
endif


all: $(TARGET)

clean:
	$(RM) $(OBJ_LIBMPI) $(DEPENDENCIES) $(OBJ_MPIRUN) lampi.$(DLLEXT)

distclean: clean
	$(RM) $(TARGET) Makefile config.h config.log config.status

install: all
	umask 002 ; \
	$(INSTALL) -m 775 -d $(prefix) ; \
	$(INSTALL) -m 775 -d $(bindir) ; \
	$(INSTALL) -m 775 -d $(includedir) ; \
	$(INSTALL) -m 775 -d $(libdir) ; \
	$(INSTALL) -m 775 -d $(sysconfdir) ; \
	$(INSTALL) -m 775 -d $(includedir)/mpi ; \
	$(INSTALL) -m 775 -d $(includedir)/ulm ; \
	$(INSTALL) -m 775 -d $(prefix)/share ; \
	$(INSTALL) -m 775 -d $(prefix)/share/man ; \
	$(INSTALL) -m 775 -d $(prefix)/share/man/man1 ; \
	$(INSTALL) -m 775 -d $(prefix)/share/lampi ; \
	$(INSTALL) -m 775 mpirun$(EXEEXT) $(bindir) ; \
	$(INSTALL) -m 664 $(srcdir)/man/lampi.man1 $(prefix)/share/man/man1/lampi.1 ; \
	$(INSTALL) -m 664 $(srcdir)/man/mpirun.man1 $(prefix)/share/man/man1/mpirun.1 ; \
	$(INSTALL) -m 664 $(srcdir)/RELEASE_NOTES $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 $(srcdir)/man/lampi.html $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 $(srcdir)/man/mpirun.html $(prefix)/share/lampi ; \
	$(INSTALL) -m 664 $(srcdir)/etc/lampi.conf $(sysconfdir)/lampi.conf ; \
	(cd $(prefix) ; test -d man || $(LN_S) share/man .) ; \
	(cd $(srcdir)/src/include ; for f in *.h; do ../../$(INSTALL) -m 664 $$f $(includedir); done) ; \
	(cd $(srcdir)/src/include/mpi ; for f in *.h; do ../../../$(INSTALL) -m 664 $$f $(includedir)/mpi; done) ; \
	(cd $(srcdir)/src/include/ulm ; for f in *.h; do ../../../$(INSTALL) -m 664 $$f $(includedir)/ulm; done)
ifeq "$(enable_dynamic)" "yes"
	umask 002; $(INSTALL) -m 775 libmpi.$(DLLEXT) $(libdir)/libmpi.$(DLLEXT)
endif
ifeq "$(enable_static)" "yes"
	umask 002; $(INSTALL) -m 664 libmpi.$(LIBEXT) $(libdir)/libmpi.$(LIBEXT)
endif
ifeq "$(with_romio)" "yes"
	umask 002 ; \
	(cd $(ROMIODIR)/include ; for f in *.h; do $(INSTALL) -m 664 $$f $(includedir); done) ; \
	test -f $(includedir)/mpio.h && mv $(includedir)/mpiandio.h $(includedir)/mpi.h
endif

check:
	@test -d src/check/mpi-ping || mkdir -p src/check/mpi-ping
	$(CC) -I$(srcdir)/src/include -L. -o src/check/mpi-ping/mpi-ping $(srcdir)/src/check/mpi-ping/mpi-ping.c -lmpi
	./mpirun -n 2 src/check/mpi-ping/mpi-ping 0 16k


mpirun$(EXEEXT): $(OBJ_MPIRUN)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_MPIRUN) -o $@ $(OBJ_MPIRUN) $(LDLIBS) $(LDLIBS_MPIRUN)

lampi.$(DLLEXT): $(OBJ_LIBMPI)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_LIBMPI) -o $@ $(OBJ_LIBMPI) $(LDLIBS) $(LDLIBS_LIBMPI)

libmpi.$(DLLEXT): $(OBJ_LIBMPI) $(ROMIOLIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDFLAGS_LIBMPI) -o $@ $(OBJ_LIBMPI) $(OBJ_ROMIO) $(LDLIBS) $(LDLIBS_LIBMPI)

libmpi.$(LIBEXT): $(OBJ_LIBMPI) $(ROMIOLIB)
	@for f in $(OBJ_LIBMPI) $(OBJ_ROMIO) ; do \
	  l=`echo $$f | tr '/' '.'`; \
	  $(LN_S) $$f lampi-1.3.X.$$l; \
	done
	$(AR) $(ARFLAGS) $@ lampi*.$(OBJEXT)
	@$(RM) lampi*.$(OBJEXT)
	$(RANLIB) $@

$(ROMIOLIB): lampi.$(DLLEXT)
	@test -d $(ROMIODIR) || mkdir $(ROMIODIR)
	@test -d $(ROMIODIR)/objects || mkdir $(ROMIODIR)/objects
	@$(RM) $(ROMIOLIB)
	(cd $(ROMIODIR) ; test -f Makefile || $(ROMIOCONFIGURE))
	(cd $(ROMIODIR) ; $(MAKE) FROM_MPICH=1 mpiolib)

# include dependency files, if we were not invoked as "make clean"
# or "make distclean" or as "make nodeps=1 ..."

ifndef nodeps
ifneq ($(findstring clean, $(MAKECMDGOALS)),clean)
-include $(DEPENDENCIES)
endif
endif

# rules for calculating dependency files

%.d: %.c
	@mkdir -p $(@D)
	$(CC) -M $(CPPFLAGS) $< | sed  -e 's@^.*\'.$(OBJEXT)':@$*'.$(OBJEXT)' $*.d:@' > $@

%.d: %.cc
	@mkdir -p $(@D)
	$(CXX) -M $(CPPFLAGS) $< | sed  -e 's@^.*\'.$(OBJEXT)':@$*'.$(OBJEXT)' $*.d:@' > $@

# rules for assembly files

%.$(OBJEXT): %.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

%.$(OBJEXT): %.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
