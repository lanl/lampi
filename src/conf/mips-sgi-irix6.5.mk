######################################################################
#
# Makefile system dependent definitions for LA-MPI
#
######################################################################

# CONFIG = mips-sgi-irix6.5 (SGI O2000, IRIX 6.5)

# special strings for text manipulation
comma   :=,

# Standard macros
CC		:= cc
CXX		:= CC 
CFLAGS		:= -64 -mips4 -LANG:ansi-for-init-scope=ON
CPPFLAGS	:= -I. -Iinclude
ifdef ULMDEBUG
CFLAGS		+= -O0 -g -LANG:restrict=ON
else
CFLAGS		+= -O3 -g3 -OPT:Olimit=0 -LANG:restrict=ON
CPPFLAGS	+= -DNDEBUG
endif
CXXFLAGS	:= $(CFLAGS) -no_auto_include
ASFLAGS		:= $(CFLAGS)
LDFLAGS		:=
LDLIBS		:=
SOSUFFIX	:= so
RANLIB		:= :	# No ranlib on irix

# target specific flags with "make rpath" support
LDFLAGS_LIBMPI	:= -shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))
LDLIBS_LIBMPI	:= -lftn

# OS defines and modules
CPPFLAGS	+=
MODULES_OS	:= os
MODULES_OS	+= os/IRIX

# hardware defines and modules
CPPFLAGS	+= -DSHARED_MEMORY
CPPFLAGS	+= -DUDP

# LSF support
USE_LSF		:= 1
ifdef USE_LSF
LSF_PREFIX=/lsf
LSF_INCLUDE_PREFIX=/lsf/share
CPPFLAGS	+= -DLSF
CPPFLAGS	+= -I$(LSF_INCLUDE_PREFIX)/include
LDFLAGS_MPIRUN	+= -L $(LSF_PREFIX)/lib64
LDLIBS_MPIRUN	+= -llsf
endif

# NUMA support
#USE_NUMA	:= 1
ifdef USE_NUMA
CPPFLAGS	+= -DNUMA
MODULES_OS	+= os/IRIX/SN0
endif

# miscellaneous defines
CPPFLAGS	+= -DRELIABILITY_ON
CPPFLAGS	+= -DCHECK_API_ARGS
#CPPFLAGS	+= -DSN0
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
