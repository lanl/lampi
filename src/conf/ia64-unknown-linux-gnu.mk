######################################################################
#
# Makefile system dependent definitions for LA-MPI
#
######################################################################

# CONFIG = ia64-unknown-linux-gnu (linux, ia64)

# special strings for text manipulation
comma	:=,

# Standard macros
ifdef ULMDEBUG
CFLAGS		+= -g
else
CFLAGS		+= -g -O
CPPFLAGS	+= -DNDEBUG
endif

ifneq (,$(findstring ecc, $(CC)))
CC		:= ecc
CXX		:= $(CC) 
CPPFLAGS	+= -I. -Iinclude -DWITH_ECC
ASFLAGS	:= -c
AS		:= $(CC)

else
CC		:= gcc
CXX		:= g++ 
CFLAGS		+= -finline-functions
CPPFLAGS	+= -Wall -Wno-deprecated -I. -Iinclude
ASFLAGS		:= $(CFLAGS)
endif

CXXFLAGS	:= $(CFLAGS)
LDFLAGS		:= 
LDLIBS		:=

SOSUFFIX	:= so
RANLIB		:= ranlib

# target specific flags with "make rpath" support
LDFLAGS_LIBMPI	:= -shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))
LDLIBS_LIBMPI	:=

# OS defines and modules
CPPFLAGS	+=
MODULES_OS	:= os
MODULES_OS	+= os/LINUX
MODULES_OS	+= os/LINUX/ia64

# hardware defines and modules
CPPFLAGS	+= -DSHARED_MEMORY
CPPFLAGS	+= -DWITH_UDP

# Quadrics RMS / Elan support
# USE_RMS	:= 1
ifdef USE_RMS
USE_QUADRICS	:= 1
#RMS_PREFIX	:= /usr
CPPFLAGS	+= -DQUADRICS -DUSE_RMS
LDLIBS		+= -lrt -lrmscall -lelan -lelan3
ifdef RMS_PREFIX
CPPFLAGS	+= -I$(RMS_PREFIX)/include
LDFLAGS		+= -L $(RMS_PREFIX)/lib
endif
endif

#GM support
ifdef USE_GM
#
# the default location is /opt/gm 
#
GM_PREFIX	:= /opt/gm-1.6.3
CPPFLAGS	+= -DWITH_GM
CPPFLAGS	+= -I$(GM_PREFIX)/include
LDFLAGS_LIBMPI	+= -L$(GM_PREFIX)/lib
LDLIBS_LIBMPI	+= -lgm
endif

# LSF support
#USE_LSF		:= 1
ifdef USE_LSF
LSF_PREFIX=/lsf
CPPFLAGS	+= -DLSF
CPPFLAGS	+= -I$(LSF_PREFIX)/share/include
LDFLAGS		+= -L $(LSF_PREFIX)/lib 
LDLIBS		+= -llsf
endif

LDLIBS		+= -lpthread

# BPROC support
#USE_BPROC	:= 1
ifdef USE_BPROC
CPPFLAGS	+= -DWITH_BPROC
LDLIBS		+= -lbproc
endif

# miscellaneous defines
CPPFLAGS	+= -DCHECK_API_ARGS
CPPFLAGS	+= -DRELIABILITY_ON
#CPPFLAGS	+= -DPURIFY
#CPPFLAGS	+= -D_DEBUGQUEUES
#CPPFLAGS	+= -D_DEBUGMEMORYSTACKS

# output options
ifdef ULM_DBG
CPPFLAGS	+= -DULM_DBG
endif
ifdef ULM_MEMPROFILE
CPPFLAGS	+= -DULM_MEMPROFILE
endif
ifdef ULM_VERBOSE
CPPFLAGS	+= -DULM_VERBOSE
endif

