######################################################################
#
# Makefile system dependent definitions for ULM MPI
#
######################################################################

# CONFIG = i686-pc-linux-gnu (linux, i686, gcc)
# Standard macros
CC		:= cc
CXX		:= g++
CFLAGS		:= -finline-functions -fno-common -no-cpp-precomp
CPPFLAGS	:= -Wall -Wno-long-double -Wno-deprecated -I. -Iinclude
ifdef ULMDEBUG
CFLAGS		+= -g
else
CFLAGS		+= -O
CPPFLAGS	+= -DNDEBUG
endif
ifdef ULM_MEMPROFILE
CFLAGS		+= -DULM_MEMPROFILE=1
else
CFLAGS		+= -DULM_MEMPROFILE=0
endif
CXXFLAGS	:= $(CFLAGS)
LDFLAGS		:= 
LDLIBS		:=

SOSUFFIX	:= dylib
RANLIB		:= ranlib

# target specific flags
LDFLAGS_LIBMPI	:= -dynamiclib -flat_namespace
LDLIBS_LIBMPI	:=

# OS defines and modules
CPPFLAGS	+=
MODULES_OS	:= os
MODULES_OS	+= os/DARWIN
MODULES_OS	+= os/common
MODULES_OS	+= os/DARWIN/powerpc

# hardware defines and modules
CPPFLAGS	+= -DSHARED_MEMORY
CPPFLAGS	+= -DUDP
ifdef USE_GM
CPPFLAGS	+= -DGM
endif

# LSF support
#USE_LSF		:= 1
ifdef USE_LSF
LSF_PREFIX	:= /lsf
CPPFLAGS	+= -DLSF
CPPFLAGS	+= -I$(LSF_PREFIX)/share/include
LDFLAGS		+= -L $(LSF_PREFIX)/lib 
LDLIBS		+= -llsf
endif

# BPROC support
#USE_BPROC		:= 1
ifdef USE_BPROC
CPPFLAGS	+= -DBPROC
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
