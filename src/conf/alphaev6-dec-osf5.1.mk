######################################################################
#
# Makefile system dependent definitions for LA-MPI
#
######################################################################

# CONFIG = alphaev6-dec-osf5.1 (Tru64, alpha, ev6, cxx)

# Standard macros
CC		:= cc

ifneq ($(CC),gcc)

CXX		:= cxx 
CFLAGS		+= -arch host -accept restrict_keyword
CPPFLAGS	+= -pthread -w -I. -Iinclude -I/usr/opt/rms/include 
ifdef ULMDEBUG
CFLAGS		+= -g -gall -trapuv
else
# benchmarks seems to do better with -O3 than -O4 and -O5
CFLAGS		+= -g3 -O3
CPPFLAGS	+= -DNDEBUG
endif
CXXFLAGS	:= $(CFLAGS) -model ansi
ASFLAGS		:= $(CFLAGS)
LDFLAGS		:= -pthread
LDLIBS		:= -lrt -lrmscall -lelan -lelan3 

else  # gcc

CXX		:= g++ 
CFLAGS		:= -Wall -mcpu=ev6
CPPFLAGS	:= -I. -Iinclude -I/usr/opt/rms/include 
ifdef ULMDEBUG
CFLAGS		+= -g
else
CFLAGS		+= -O3
CPPFLAGS	+= -DNDEBUG
endif
CXXFLAGS	:= $(CFLAGS)
ASFLAGS		:= $(CFLAGS)
CPPFLAGS	+= -DGCCBUG
LDFLAGS		:=
LDLIBS		:= -lrt -lrmscall -lelan -lelan3 

endif

SOSUFFIX	:= so
RANLIB		:= ranlib

# target specific flags with "make rpath" support
LDFLAGS_LIBMPI	:= -shared -all $(patsubst %,-rpath %,$(rpath))
LDLIBS_LIBMPI	:=

# OS defines and modules
CPPFLAGS	+=
MODULES_OS	:= os
MODULES_OS	+= os/TRU64

# hardware defines and modules
CPPFLAGS	+= -DWITH_UDP
CPPFLAGS	+= -DQUADRICS
CPPFLAGS	+= -DSHARED_MEMORY

# miscellaneous defines
CPPFLAGS	+= -DRELIABILITY_ON
CPPFLAGS	+= -DCHECK_API_ARGS
ifdef ULM_MEMPROFILE
CPPFLAGS	+= -DULM_MEMPROFILE
endif
#CPPFLAGS	+= -DPURIFY
#CPPFLAGS	+= -D_DEBUGQUEUES
#CPPFLAGS	+= -D_DEBUGMEMORYSTACKS

# LSF support
#USE_LSF		:= 1
ifdef USE_LSF
LSF_PREFIX=/lsf
CPPFLAGS	+= -DLSF
CPPFLAGS	+= -I$(LSF_PREFIX)/share/include
LDFLAGS		+= -L$(LSF_PREFIX)/lib 
LDLIBS		+= -llsf
endif

# Quadrics RMS support
USE_RMS		:= 1
ifdef USE_RMS
CPPFLAGS	+= -DUSE_RMS
endif

# BPROC support
#USE_BPROC		:= 1
ifdef USE_BPROC
CPPFLAGS	+= -DWITH_BPROC
LDLIBS		+= -lbproc
endif

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

