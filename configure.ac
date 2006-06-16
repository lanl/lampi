# Process this file with autoconf to produce a configure script.

AC_INIT(lampi,1.5.15-rc4, lampi-support@lanl.gov)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([conf])
AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_F77(lf77 lf90 lf95 pgf77 pgf90 xlf xlf90 f77 f90 fort77)
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB(aio, aio_error)
AC_CHECK_LIB(rt, aio_error)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([byteswap.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([pty.h])
AC_CHECK_HEADERS([sched.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/uio.h])
AC_CHECK_HEADERS([sys/utsname.h])
AC_CHECK_HEADERS([util.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_CHECK_TYPES([ptrdiff_t])

AC_MSG_CHECKING([for socklen_t])
AC_TRY_COMPILE([
 #include <sys/types.h>
 #include <sys/socket.h>
],
  [socklen_t x;],
  AC_MSG_RESULT([yes]),
  [AC_MSG_RESULT([no])
  AC_DEFINE(socklen_t, int,
	[Define to int if <sys/types.h> or <sys/socket.h> does not define])]
)

AC_MSG_CHECKING([for pragma weak])
AC_LANG_CONFTEST([[
#pragma weak fake = real
int fake(int i);
int real(int i) { return i; }
]])
$CC $CFLAGS -c conftest.c 2>/dev/null
if nm conftest.o | grep fake > /dev/null ; then
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_PRAGMA_WEAK, 1)
else
    AC_MSG_RESULT([no])
fi

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_GETPGRP
AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([alarm])
AC_CHECK_FUNCS([atexit])
AC_CHECK_FUNCS([bzero])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([ftruncate])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([gethostbyaddr])
AC_CHECK_FUNCS([gethostbyname])
AC_CHECK_FUNCS([gethostname])
AC_CHECK_FUNCS([getpagesize])
AC_CHECK_FUNCS([getrusage])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkfifo])
AC_CHECK_FUNCS([munmap])
AC_CHECK_FUNCS([putenv])
AC_CHECK_FUNCS([sched_setaffinity])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strcspn])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strsignal])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoul])
AC_CHECK_FUNCS([waitpid])
AC_CHECK_FUNCS([wait3])

# Default modules (subdirectories) to compile for libmpi.

MODULES_LIBMPI="$MODULES_LIBMPI src/client"
MODULES_LIBMPI="$MODULES_LIBMPI src/collective"
MODULES_LIBMPI="$MODULES_LIBMPI src/collective/p2p"
MODULES_LIBMPI="$MODULES_LIBMPI src/init"
MODULES_LIBMPI="$MODULES_LIBMPI src/interface"
MODULES_LIBMPI="$MODULES_LIBMPI src/mem"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/c"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/c2f"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/f"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/internal"
MODULES_LIBMPI="$MODULES_LIBMPI src/path/common"
MODULES_LIBMPI="$MODULES_LIBMPI src/queue"
MODULES_LIBMPI="$MODULES_LIBMPI src/util"

# Default modules (subdirectories) to compile for mpirun.

MODULES_MPIRUN="$MODULES_MPIRUN src/client"
MODULES_MPIRUN="$MODULES_MPIRUN src/init"
MODULES_MPIRUN="$MODULES_MPIRUN src/interface"
MODULES_MPIRUN="$MODULES_MPIRUN src/run"
MODULES_MPIRUN="$MODULES_MPIRUN src/util"

# Defaults compilation/link options for all hosts.

LIBEXT="a"
DLLEXT="so"

romiodir=romio-1.2.5.1
ROMIO_FS="ufs nfs"
ROMIO_CONFIG_OPTIONS=""

# Default features

enable_check_api_args=${enable_check_api_args:-yes}
enable_dynamic=${enable_dynamic:-yes}
enable_pty_stdio=${enable_pty_stdio:-yes}
enable_reliability=${enable_reliability:-yes}
enable_shared_memory=${enable_shared_memory:-yes}
enable_static=${enable_static:-yes}

enable_bproc=${enable_bproc:-no}
enable_dbg=${enable_dbg:-no}
enable_debug=${enable_debug:-no}
enable_gm=${enable_gm:-no}
enable_ib=${enable_ib:-no}
enable_large_file_support=${enable_large_file_support:-no}
enable_lsf=${enable_lsf:-no}
enable_memprofile=${enable_memprofile:-no}
enable_numa=${enable_numa:-no}
enable_qsnet=${enable_qsnet:-no}
enable_rms=${enable_rms:-no}
enable_static=${enable_static:-no}
enable_syslog=${enable_syslog:-no}
enable_tcp=${enable_tcp:-no}
enable_udp=${enable_udp:-no}

with_romio=${with_romio:-no}

# Host dependent features.

case "${host}" in

i?86-pc-linux*)
        if test "$CC" = "icc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _LDFLAGS_LIBMPI='-shared'
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _CFLAGS="$_CFLAGS -march=i686 -finline-functions"
           _CFLAGS="$_CFLAGS -Wall"
           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        _CXXFLAGS="$_CFLAGS -Wno-deprecated"

        if test "$enable_pty_stdio" = "yes" ; then
           LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lutil"
        fi

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/LINUX"
        MODULES_OS="$MODULES_OS src/os/LINUX/i686"

        # default features for this host

        # default paths for this host
        IB_PREFIX="${IB_PREFIX:-/usr/mellanox}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

        #default ROMIO filesystems:
        ROMIO_FS="ufs nfs panfs"

	;;

i?86-pc-cygwin*)
        if test "$CC" = "icc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _LDFLAGS_LIBMPI='-shared'
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _CFLAGS="$_CFLAGS -march=i686 -finline-functions"
           _CFLAGS="$_CFLAGS -Wall"
           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        _CXXFLAGS="$_CFLAGS -Wno-deprecated"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/CYGWIN"

        # default features for this host

        # default paths for this host
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

	;;

ia64-unknown-linux*)
        if test "$CC" = "ecc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _CFLAGS="$_CFLAGS -restrict"
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _CFLAGS="$_CFLAGS -finline-functions"
           _CFLAGS="$_CFLAGS -Wall"
           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        _CXXFLAGS="$_CFLAGS -Wno-deprecated"

        if test "$enable_pty_stdio" = "yes" ; then
           LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lutil"
        fi

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/LINUX"
        MODULES_OS="$MODULES_OS src/os/LINUX/ia64"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

        #default ROMIO filesystems:
        ROMIO_FS="ufs nfs panfs"

	;;

x86_64-unknown-linux*)
        CC="gcc"
        CXX="g++"
        if test "$enable_debug" = "yes" ; then
           _CFLAGS="$_CFLAGS -g"
        else
           _CFLAGS="$_CFLAGS -g -O"
        fi
        _CFLAGS="$_CFLAGS -finline-functions -fPIC"
	_CFLAGS="$_CFLAGS -Wall"
        # link flags with "make rpath=" support
        _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        _CXXFLAGS="$_CFLAGS -Wno-deprecated"

        if test "$enable_pty_stdio" = "yes" ; then
           LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lutil"
        fi

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/LINUX"
        MODULES_OS="$MODULES_OS src/os/LINUX/x86_64"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

        #default ROMIO filesystems:
        ROMIO_FS="ufs nfs panfs"
        ROMIO_CONFIG_OPTIONS="$ROMIO_CONFIG_OPTIONS -cflags='-fPIC'"

	;;

alpha*-dec*)
        if test "$CC" = "gcc" ; then
           CXX="g++"
           _CFLAGS="$_CFLAGS -Wall -mcpu=ev6"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -O3"
           fi
           _CXXFLAGS="$_CFLAGS"

           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        else
           CXX="cxx"
           _CFLAGS="$_CFLAGS -arch host -accept restrict_keyword"
           _CPPFLAGS="-w"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g -gall -trapuv"
           else
              # benchmarks seems to do better with -O3 than -O4 and -O5
              _CFLAGS="$_CFLAGS -g3 -O3"
           fi
           _CXXFLAGS="$_CFLAGS -model ansi"

           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared -all $(patsubst %,-rpath %,$(rpath)) -none'
           if test "$with_romio" = "yes" ; then
              LDLIBS_LIBMPI="$LDLIBS_LIBMPI -laio"
           fi
        fi

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/TRU64"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"

        # on ASCI-Q, header files are broken. Use alternate ones.
        RMS_PREFIX="${RMS_PREFIX:-/usr/opt/rms}"
        ROMIO_FS="ufs nfs cfs"

	;;

powerpc-apple-darwin*)
        PPC_TYPE=`/usr/bin/machine`
        CC="gcc"
        CXX="g++"
        if test "$enable_debug" = "yes" ; then
           _CFLAGS="$_CFLAGS -g"
        else
           _CFLAGS="$_CFLAGS -g -O"
        fi
        _CFLAGS="$_CFLAGS -finline-functions -fno-common -Wall"
        AC_MSG_CHECKING([for -Wno-long-double])
        AC_LANG_CONFTEST([[ int a(void) { return 0; } ]])
        if $CC -Wno-long-double -c conftest.c 2>/dev/null ; then
           _CFLAGS="$_CFLAGS -Wno-long-double -no-cpp-precomp"
           AC_MSG_RESULT([yes])
        else
           AC_MSG_RESULT([no])
        fi
        DLLEXT="dylib"
        CURRENT_VERSION=`echo $PACKAGE_VERSION | sed -e 's/-.*$//'`
        _LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI -dynamiclib -flat_namespace -current_version $CURRENT_VERSION -compatibility_version 1.0.0"
        ROMIO_CONFIG_OPTIONS="$ROMIO_CONFIG_OPTIONS -arch=freebsd"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/DARWIN"
        if test "$PPC_TYPE" = "ppc970" ; then
            _CFLAGS="$_CFLAGS $CFLAGS -mcpu=970 -mtune=970 -mpowerpc64 -mpowerpc-gpopt"
            _CPPFLAGS="-D__ppc_64__ -force_cpusubtype_ALL"
            MODULES_OS="$MODULES_OS src/os/DARWIN/ppc_64"
        else
            MODULES_OS="$MODULES_OS src/os/DARWIN/powerpc"
        fi
        _CXXFLAGS="$_CFLAGS -Wno-deprecated"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB=""
        LSF_INC=""
        RMS_PREFIX=""

	;;

mips-sgi-irix*)
        if test "$CC" = "cc" ; then
           CXX="CC"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -O0 -g -LANG:restrict=ON"
           else
              _CFLAGS="$_CFLAGS -O3 -g3 -OPT:Olimit=0 -LANG:restrict=ON"
           fi
           _CFLAGS="$_CFLAGS -64 -mips4 -LANG:ansi-for-init-scope=ON"
           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
           LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lftn"
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              _CFLAGS="$_CFLAGS -g"
           else
              _CFLAGS="$_CFLAGS -g -O"
           fi
           _CFLAGS="$_CFLAGS -finline-functions"
           _CFLAGS="$_CFLAGS -Wall"
           # link flags with "make rpath=" support
           _LDFLAGS_LIBMPI='-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        _CXXFLAGS="-no_auto_include -Wno-deprecated $_CFLAGS"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/IRIX"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

	;;

rs6000-ibm-aix*)
        AC_MSG_ERROR([Support for ${host} not implemented])
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/AIX"
	;;

sparc-sun-solaris2.*)
	AC_MSG_ERROR([Support for ${host} not implemented])
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/SOLARIS"
	;;

*)
        AC_MSG_ERROR([Unknown system type... ${host}])
	;;
esac

# Append user supplied variables to standard flags

if test "$ac_test_CFLAGS" = "set" ; then
   CFLAGS="$_CFLAGS $ac_save_CFLAGS"
else
   CFLAGS="$_CFLAGS"
fi
if test "$ac_test_CPPFLAGS" = "set" ; then
   CPPFLAGS="$_CPPFLAGS $ac_save_CPPFLAGS"
else
   CPPFLAGS="$_CPPFLAGS"
fi
if test "$ac_test_CXXFLAGS" = "set" ; then
   CXXFLAGS="$_CXXFLAGS $ac_save_CXXFLAGS"
else
   CXXFLAGS="$_CXXFLAGS"
fi

LDFLAGS="$_LDFLAGS $LDFLAGS"
LDLIBS="$_LDLIBS $LDLIBS $LIBS"

LDFLAGS_LIBMPI="$_LDFLAGS_LIBMPI $LDFLAGS_LIBMPI"
LDLIBS_LIBMPI="$_LDLIBS_LIBMPI $LDLIBS_LIBMPI"

LDFLAGS_MPIRUN="$_LDFLAGS_MPIRUN $LDFLAGS_MPIRUN"
LDLIBS_MPIRUN="$_LDLIBS_MPIRUN $LDLIBS_MPIRUN"

ROMIO_CONFIG_OPTIONS="$ROMIO_CONFIG_OPTIONS -cflags='$CFLAGS $CPPFLAGS'"

# Optional features

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug], [enable debugging (default is no)]))
echo "debug build... ${enable_debug:-no}"
if test "$enable_debug" = "no" ; then
   AC_DEFINE(NDEBUG, 1)
   AC_DEFINE(ENABLE_DEBUG, 0)
else
   AC_DEFINE(ENABLE_DEBUG, 1)
fi

AC_ARG_ENABLE(static,
              AC_HELP_STRING([--enable-static], [enable installation of static library]))
echo "static library... ${enable_static:-no}"

AC_ARG_ENABLE(dynamic,
              AC_HELP_STRING([--enable-dynamic], [enable installation of dynamic library]))
echo "dynamic library... ${enable_dynamic:-no}"

AC_ARG_ENABLE(lsf,
              AC_HELP_STRING([--enable-lsf], [use LSF]))
echo "feature LSF... ${enable_lsf:-no}"
if test "$enable_lsf" = "yes" ; then
   AC_DEFINE(ENABLE_LSF, 1)
   test "$LSF_INC" = "" || CPPFLAGS="$CPPFLAGS -I$LSF_INC"
   test "$LSF_LIB" = "" || LDFLAGS="$LDFLAGS -L$LSF_LIB"
   LIBS="$LIBS -llsf -lnsl"
else
   AC_DEFINE(ENABLE_LSF, 0)
fi

AC_ARG_ENABLE(rms,
              AC_HELP_STRING([--enable-rms], [use RMS]))
echo "feature RMS... ${enable_rms:-no}"
if test "$enable_rms" = "yes" ; then
   AC_DEFINE(ENABLE_RMS, 1)
   test "$RMS_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$RMS_PREFIX/include"
   test "$RMS_PREFIX" = "" || LDFLAGS="$LDFLAGS -L$RMS_PREFIX/lib"
   LIBS="$LIBS -lrt -lrmscall -lelan -lelan3 -lelanctrl"
else
   AC_DEFINE(ENABLE_RMS, 0)
fi

AC_ARG_ENABLE(bproc,
              AC_HELP_STRING([--enable-bproc], [use BPROC]))
echo "feature BPROC... ${enable_bproc:-no}"
if test "$enable_bproc" = "yes" ; then
   AC_DEFINE(ENABLE_BPROC, 1)
   AC_CHECK_MEMBER(struct bproc_io_t.send_info,[AC_DEFINE(BPROC_MAJOR_VERSION, 3)],
    [AC_DEFINE(BPROC_MAJOR_VERSION, 4)],[#include <sys/bproc.h>])
   LIBS="$LIBS -lbproc"
else
   AC_DEFINE(ENABLE_BPROC, 0)
fi

AC_ARG_ENABLE(udp,
              AC_HELP_STRING([--enable-udp], [enable UDP path]))
echo "feature UDP... ${enable_udp:-no}"
if test "$enable_udp" = "yes" ; then
   AC_DEFINE(ENABLE_UDP, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/udp"
else
   AC_DEFINE(ENABLE_UDP, 0)
fi

AC_ARG_ENABLE(tcp,
              AC_HELP_STRING([--enable-tcp], [enable TCP path]))
echo "feature TCP... ${enable_tcp:-no}"
if test "$enable_tcp" = "yes" ; then
   AC_DEFINE(ENABLE_TCP, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/tcp"
else
   AC_DEFINE(ENABLE_TCP, 0)
fi

AC_ARG_ENABLE(qsnet,
              AC_HELP_STRING([--enable-qsnet], [enable QSNET path]))
echo "feature QSNET... ${enable_qsnet:-no}"
if test "$enable_qsnet" = "yes" ; then
   AC_DEFINE(ENABLE_QSNET, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/quadrics"
   MODULES_LIBMPI="$MODULES_LIBMPI src/collective/quadrics"
   case $LIBS in
   *-lelan*)
      ;;
   *)
      LIBS="$LIBS -lelan -lelan3"
      ;;
   esac
else
   AC_DEFINE(ENABLE_QSNET, 0)
fi

AC_ARG_ENABLE(gm,
              AC_HELP_STRING([--enable-gm], [enable Myrinet GM path]))
echo "feature GM... ${enable_gm:-no}"
if test "$enable_gm" = "yes" ; then
   AC_DEFINE(ENABLE_GM, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/gm"
   test "$GM_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$GM_PREFIX/include"
   test "$GM_PREFIX" = "" || LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI -L$GM_PREFIX/lib"
   LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lgm"
else
   AC_DEFINE(ENABLE_GM, 0)
fi

AC_ARG_ENABLE(ib,
              AC_HELP_STRING([--enable-ib], [enable InfiniBand path]))
echo "feature InfiniBand... ${enable_ib:-no}"
if test "$enable_ib" = "yes" ; then
   AC_DEFINE(ENABLE_INFINIBAND, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/ib"
   test "$IB_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$IB_PREFIX/include"
   test "$IB_PREFIX" = "" || LDFLAGS="$LDFLAGS -L$IB_PREFIX/lib"
#   LIBS="$LIBS -Wl,--allow-multiple-definition -Wl,-Bstatic -lvapi -lmosal -lmpga -lmtl_common -Wl,-Bdynamic"
   LIBS="$LIBS -lvapi -lmosal -lmpga -lmtl_common"
else
   AC_DEFINE(ENABLE_INFINIBAND, 0)
fi

AC_ARG_ENABLE(shared_memory,
              AC_HELP_STRING([--enable-shared-memory], [enable shared memory path (probably required)]))
echo "feature SHARED_MEMORY... ${enable_shared_memory:-no}"
if test "$enable_shared_memory" = "yes" ; then
   AC_DEFINE(ENABLE_SHARED_MEMORY, 1)
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/sharedmem"
else
   AC_DEFINE(ENABLE_SHARED_MEMORY, 0)
fi

AC_ARG_ENABLE(reliability,
              AC_HELP_STRING([--enable-reliability], [enable reliablity]))
echo "feature RELIABILITY... ${enable_reliability:-no}"
if test "$enable_reliability" = "yes" ; then
   AC_DEFINE(ENABLE_RELIABILITY, 1)
else
   AC_DEFINE(ENABLE_RELIABILITY, 0)
fi

AC_ARG_ENABLE(large_file_support,
              AC_HELP_STRING([--enable-large-file-support], [enable large file support]))
echo "feature LARGE_FILE_SUPPORT... ${enable_large_file_support:-no}"
if test "$enable_large_file_support" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGEFILE_SOURCE=1"
   ROMIO_CONFIG_OPTIONS="$ROMIO_CONFIG_OPTIONS -cflags='-D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1 -D_LARGEFILE_SOURCE=1'"
fi

AC_ARG_ENABLE(pty_stdio,
              AC_HELP_STRING([--enable-pty-stdio], [enable use of pty's for stdio redirection]))
echo "feature PTY_STDIO... ${enable_pty_stdio:-no}"
if test "$enable_pty_stdio" = "yes" ; then
   AC_DEFINE(ENABLE_PTY_STDIO, 1)
else
   AC_DEFINE(ENABLE_PTY_STDIO, 0)
fi

AC_ARG_ENABLE(check_api_args,
              AC_HELP_STRING([--enable-check-api-args], [enable checking of API arguments]))
echo "feature CHECK_API_ARGS... ${enable_check_api_args:-no}"
if test "$enable_check_api_args" = "yes" ; then
   AC_DEFINE(ENABLE_CHECK_API_ARGS, 1)
else
   AC_DEFINE(ENABLE_CHECK_API_ARGS, 0)
fi

AC_ARG_ENABLE(enable-dbg,
              AC_HELP_STRING([--enable-dbg], [enable copious debugging output]))
echo "copious debugging output... ${enable_dbg:-no}"
if test "$enable_dbg" = "yes" ; then
   AC_DEFINE(ENABLE_DBG, 1)
else
   AC_DEFINE(ENABLE_DBG, 0)
fi

AC_ARG_ENABLE(enable-syslog,
              AC_HELP_STRING([--enable-syslog], [enable syslog recording of jobs]))
echo "syslog recording of jobs... ${enable_syslog:-no}"
if test "$enable_syslog" = "yes" ; then
   AC_DEFINE(ENABLE_SYSLOG, 1)
else
   AC_DEFINE(ENABLE_SYSLOG, 0)
fi

AC_ARG_ENABLE(memprofile,
              AC_HELP_STRING([--enable-memprofile], [enable memory profiling]))
echo "feature MEMPROFILE... ${enable_memprofile:-no}"
if test "$enable_memprofile" = "yes" ; then
   AC_DEFINE(ENABLE_MEMPROFILE, 1)
else
   AC_DEFINE(ENABLE_MEMPROFILE, 0)
fi

AC_ARG_WITH(romio,
            AC_HELP_STRING([--with-romio], [include MPI-IO support]))
echo "package ROMIO... ${with_romio:-no}"

AC_MSG_CHECKING([makefile configuration include])

# autoheader templates

AH_TEMPLATE([HAVE_PRAGMA_WEAK],
            [Define to 1 if the system supports "pragma weak".])

AH_TEMPLATE([NDEBUG],
            [Define to 1 to disable assertions.])

AH_TEMPLATE([ENABLE_SHARED_MEMORY],
            [Define to 1 if the shared memory path is enabled.])

AH_TEMPLATE([ENABLE_LSF],
            [Define to 1 if LSF is enabled.])

AH_TEMPLATE([ENABLE_RMS],
            [Define to 1 if RMS is enabled.])

AH_TEMPLATE([ENABLE_BPROC],
            [Define to 1 if BPROC is enabled.])

AH_TEMPLATE([BPROC_MAJOR_VERSION],
            [Define to 3 or 4 if BPROC is enabled.])

AH_TEMPLATE([ENABLE_UDP],
            [Define to 1 if the UDP/IP path is enabled.])

AH_TEMPLATE([ENABLE_TCP],
            [Define to 1 if the TCP/IP path is enabled.])

AH_TEMPLATE([ENABLE_QSNET],
            [Define to 1 if the Quadrics QsNet path is enabled.])

AH_TEMPLATE([ENABLE_GM],
            [Define to 1 if the Myrinet GM path is enabled.])

AH_TEMPLATE([ENABLE_INFINIBAND],
            [Define to 1 if the InfiniBand path is enabled.])

AH_TEMPLATE([ENABLE_RELIABILITY],
            [Define to 1 if reliability is enabled.])

AH_TEMPLATE([ENABLE_PTY_STDIO],
            [Define to 1 if use of pty's for stdio redirection is enabled.])

AH_TEMPLATE([ENABLE_CHECK_API_ARGS],
            [Define to 1 if MPI argument checking is enabled.])

AH_TEMPLATE([ENABLE_DEBUG],
            [Define to 1 if compiled with debugging.])

AH_TEMPLATE([ENABLE_DBG],
            [Define to 1 if copious debug output is enabled.])

AH_TEMPLATE([ENABLE_SYSLOG],
            [Define to 1 if syslog recording of jobs is enabled.])

AH_TEMPLATE([ENABLE_MEMPROFILE],
            [Define to 1 if memory profiling is enabled.])

# output

AC_SUBST(F90)
AC_SUBST(LDFLAGS_LIBMPI)
AC_SUBST(LDLIBS_LIBMPI)
AC_SUBST(LDFLAGS_MPIRUN)
AC_SUBST(LDLIBS_MPIRUN)
AC_SUBST(LN_S)
AC_SUBST(MODULES_LIBMPI)
AC_SUBST(MODULES_MPIRUN)
AC_SUBST(MODULES_OS)
AC_SUBST(EXEEXT)
AC_SUBST(OBJEXT)
AC_SUBST(LIBEXT)
AC_SUBST(DLLEXT)
AC_SUBST(enable_bproc)
AC_SUBST(enable_check_api_args)
AC_SUBST(enable_dbg)
AC_SUBST(enable_debug)
AC_SUBST(enable_dynamic)
AC_SUBST(enable_gm)
AC_SUBST(enable_ib)
AC_SUBST(enable_large_file_support)
AC_SUBST(enable_lsf)
AC_SUBST(enable_memprofile)
AC_SUBST(enable_numa)
AC_SUBST(enable_pty_stdio)
AC_SUBST(enable_qsnet)
AC_SUBST(enable_reliability)
AC_SUBST(enable_rms)
AC_SUBST(enable_shared_memory)
AC_SUBST(enable_static)
AC_SUBST(enable_tcp)
AC_SUBST(enable_udp)
AC_SUBST(with_romio)
AC_SUBST(romiodir)
AC_SUBST(ROMIO_FS)
AC_SUBST(ROMIO_CONFIG_OPTIONS)

AC_OUTPUT([Makefile])
