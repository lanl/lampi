# Process this file with autoconf to produce a configure script.

AC_INIT(lampi,1.5.x,lampi-support@lanl.gov)
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
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_GETPGRP
AC_FUNC_MALLOC
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
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkfifo])
AC_CHECK_FUNCS([munmap])
AC_CHECK_FUNCS([putenv])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strcspn])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoul])

# Default modules (subdirectories) to compile for libmpi.

MODULES_LIBMPI="$MODULES_LIBMPI src/client"
MODULES_LIBMPI="$MODULES_LIBMPI src/collective"
MODULES_LIBMPI="$MODULES_LIBMPI src/collective/p2p"
MODULES_LIBMPI="$MODULES_LIBMPI src/ctnetwork"
MODULES_LIBMPI="$MODULES_LIBMPI src/init"
MODULES_LIBMPI="$MODULES_LIBMPI src/interface"
MODULES_LIBMPI="$MODULES_LIBMPI src/mem"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/c"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/c2f"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/f"
MODULES_LIBMPI="$MODULES_LIBMPI src/mpi/internal"
MODULES_LIBMPI="$MODULES_LIBMPI src/path/common"
MODULES_LIBMPI="$MODULES_LIBMPI src/queue"
MODULES_LIBMPI="$MODULES_LIBMPI src/threads"
MODULES_LIBMPI="$MODULES_LIBMPI src/util"

# Default modules (subdirectories) to compile for mpirun.

MODULES_MPIRUN="$MODULES_MPIRUN src/client"
MODULES_MPIRUN="$MODULES_MPIRUN src/ctnetwork"
MODULES_MPIRUN="$MODULES_MPIRUN src/init"
MODULES_MPIRUN="$MODULES_MPIRUN src/interface"
MODULES_MPIRUN="$MODULES_MPIRUN src/popt"
MODULES_MPIRUN="$MODULES_MPIRUN src/run"
MODULES_MPIRUN="$MODULES_MPIRUN src/threads"
MODULES_MPIRUN="$MODULES_MPIRUN src/util"

# Defaults compilation/link options for all hosts.

LDFLAGS_LIBMPI=""
LDLIBS_LIBMPI=""
LDFLAGS_MPIRUN=""
LDLIBS_MPIRUN=""
LIBEXT="a"
DLLEXT="so"
RMS_BROKEN_PREFIX=""

romiodir=romio-1.2.5.1
ROMIO_FS="ufs nfs"
              
# Default features

enable_shared_memory=${enable_shared_memory:-yes}
enable_reliability=${enable_reliability:-yes}
enable_check_api_args=${enable_check_api_args:-yes}
enable_dynamic=${enable_dynamic:-yes}
enable_static=${enable_static:-yes}

# Host dependent features.

case "${host}" in

i?86-pc-linux*)
        if test "$CC" = "icc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared'
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -march=i686 -finline-functions"
           CPPFLAGS="$CPPFLAGS -Wall -Wno-deprecated"
           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        CXXFLAGS="$CFLAGS"
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/LINUX"
        MODULES_OS="$MODULES_OS src/os/LINUX/i686"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm-1.6.3}"
        IB_PREFIX="${IB_PREFIX:-/usr/mellanox}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

	;;

i?86-pc-cygwin*)
        if test "$CC" = "icc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared'
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -march=i686 -finline-functions"
           CPPFLAGS="$CPPFLAGS -Wall -Wno-deprecated"
           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        CXXFLAGS="$CFLAGS"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/CYGWIN"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm-1.6.3}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"
        RMS_PREFIX=""

	;;

ia64-unknown-linux*)
        if test "$CC" = "ecc" ; then
           CXX="$CC"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -restrict"
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -finline-functions"
           CPPFLAGS="$CPPFLAGS -Wall -Wno-deprecated"
           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        CXXFLAGS="$CFLAGS"
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"

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

	;;

x86_64-unknown-linux*)
        CC="gcc"
        CXX="g++"
        if test "$enable_debug" = "yes" ; then
           CFLAGS="-g"
        else
           CFLAGS="-g -O"
           CPPFLAGS="$CPPFLAGS -DNDEBUG"
        fi
	#CFLAGS="$CFLAGS -finline-functions -m32"
        CFLAGS="$CFLAGS -finline-functions -fPIC"
	CPPFLAGS="$CPPFLAGS -Wall -Wno-deprecated"
        # link flags with "make rpath=" support
        LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        CXXFLAGS="$CFLAGS"
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"

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

	;;

alpha*-dec*)
        if test "$CC" = "gcc" ; then
           CXX="g++"
           CFLAGS="-Wall -mcpu=ev6"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="$CFLAGS -g"
           else
              CFLAGS="$CFLAGS -O3"
              CPPFLAGS="-DNDEBUG"
           fi
           CXXFLAGS="$CFLAGS"

           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        else
           CXX="cxx"
           CFLAGS="-arch host -accept restrict_keyword"
           CPPFLAGS="$CPPFLAGS -pthread -w"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="$CFLAGS -g -gall -trapuv"
           else
              # benchmarks seems to do better with -O3 than -O4 and -O5
              CFLAGS="$CFLAGS -g3 -O3"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CXXFLAGS="$CFLAGS -model ansi"

           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared -all $(patsubst %,-rpath %,$(rpath)) -none'
           if test "$with_romio" = "yes" ; then
              LDLIBS_LIBMPI="$LDLIBS_LIBMPI -laio"
           fi
        fi
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/TRU64"

        # default features for this host

        # default paths for this host
        GM_PREFIX="${GM_PREFIX:-/opt/gm}"
        LSF_LIB="/lsf/lib"
        LSF_INC="/lsf/share/include"

        # on ASCI-Q, header files are broken. Use alternate ones.
        RMS_BROKEN_PREFIX="/usr/local/compaq/test"
        RMS_PREFIX="${RMS_PREFIX:-/usr/opt/rms}"
        ROMIO_FS="ufs nfs cfs"

	;;

powerpc-apple-darwin*)
        PPC_TYPE=`/usr/bin/machine`
        CC="gcc"
        CXX="g++"
        if test "$enable_debug" = "yes" ; then
           CFLAGS="-g"
        else
           CFLAGS="-g -O"
           CPPFLAGS="$CPPFLAGS -DNDEBUG"
        fi
        CFLAGS="$CFLAGS -finline-functions -fno-common -no-cpp-precomp"
        CPPFLAGS="$CPPFLAGS -Wall -Wno-long-double"
        DLLEXT="dylib"
        LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI -dynamiclib -flat_namespace"

        # OS defines and modules
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/DARWIN"
        if test "$PPC_TYPE" = "ppc970" ; then
            CFLAGS="$CFLAGS -mcpu=970 -mtune=970 -mpowerpc64 -mpowerpc-gpopt"
            CPPFLAGS="$CPPFLAGS -D__ppc_64__ -force_cpusubtype_ALL"
            MODULES_OS="$MODULES_OS src/os/DARWIN/ppc_64"
        else
            MODULES_OS="$MODULES_OS src/os/DARWIN/powerpc"
        fi

        CXXFLAGS="$CFLAGS -Wno-deprecated"

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
              CFLAGS="-O0 -g -LANG:restrict=ON"
           else
              CFLAGS="-O3 -g3 -OPT:Olimit=0 -LANG:restrict=ON"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -64 -mips4 -LANG:ansi-for-init-scope=ON"
           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
           LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lftn"
        else
           CC="gcc"
           CXX="g++"
           if test "$enable_debug" = "yes" ; then
              CFLAGS="-g"
           else
              CFLAGS="-g -O"
              CPPFLAGS="$CPPFLAGS -DNDEBUG"
           fi
           CFLAGS="$CFLAGS -finline-functions"
           CPPFLAGS="$CPPFLAGS -Wall -Wno-deprecated"
           # link flags with "make rpath=" support
           LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI "'-shared $(patsubst %,-Wl$(comma)-rpath$(comma)%,$(subst :, ,$(rpath)))'
        fi
        CXXFLAGS="$CFLAGS -no_auto_include"
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"

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
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/AIX"
	;;

sparc-sun-solaris2.*)
	AC_MSG_ERROR([Support for ${host} not implemented])
        CPPFLAGS="$CPPFLAGS -DHAVE_PRAGMA_WEAK"
        MODULES_OS="$MODULES_OS src/os"
        MODULES_OS="$MODULES_OS src/os/SOLARIS"
	;;

*)
       	AC_MSG_ERROR([Unknown system type... ${host}])
	;;
esac

# Optional features

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug], [enable debugging (default is no)]))
echo "debug build... ${enable_debug:-no}"

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
   CPPFLAGS="$CPPFLAGS -DENABLE_LSF"
   test "$LSF_INC" = "" || CPPFLAGS="$CPPFLAGS -I$LSF_INC"
   test "$LSF_LIB" = "" || LDFLAGS="$LDFLAGS -L$LSF_LIB"
   LIBS="$LIBS -llsf"
fi

AC_ARG_ENABLE(rms,
              AC_HELP_STRING([--enable-rms], [use RMS]))
echo "feature RMS... ${enable_rms:-no}"
if test "$enable_rms" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_RMS"   
   test "$RMS_BROKEN_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$RMS_BROKEN_PREFIX"
   test "$RMS_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$RMS_PREFIX/include"
   test "$RMS_PREFIX" = "" || LDFLAGS="$LDFLAGS -L$RMS_PREFIX/lib"
   LIBS="$LIBS -lrt -lrmscall -lelan -lelan3"
fi

AC_ARG_ENABLE(bproc,
              AC_HELP_STRING([--enable-bproc], [use BPROC]))
echo "feature BPROC... ${enable_bproc:-no}"
if test "$enable_bproc" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_BPROC "
   LIBS="$LIBS -lbproc"
fi

AC_ARG_ENABLE(udp,
              AC_HELP_STRING([--enable-udp], [enable UDP path]))
echo "feature UDP... ${enable_udp:-no}"
if test "$enable_udp" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_UDP"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/udp"
fi

AC_ARG_ENABLE(tcp,
              AC_HELP_STRING([--enable-tcp], [enable TCP path]))
echo "feature TCP... ${enable_tcp:-no}"
if test "$enable_tcp" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_TCP"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/tcp"
fi

AC_ARG_ENABLE(qsnet,
              AC_HELP_STRING([--enable-qsnet], [enable QSNET path]))
echo "feature QSNET... ${enable_qsnet:-no}"
if test "$enable_qsnet" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_QSNET"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/quadrics"
   MODULES_LIBMPI="$MODULES_LIBMPI src/collective/quadrics"
   case $LIBS in
   *-lelan*)
      ;;
   *)
      LIBS="$LIBS -lelan -lelan3"
      ;;
   esac
fi

AC_ARG_ENABLE(gm,
              AC_HELP_STRING([--enable-gm], [enable Myrinet GM path]))
echo "feature GM... ${enable_gm:-no}"
if test "$enable_gm" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_GM"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/gm"
   test "$GM_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$GM_PREFIX/include"
   test "$GM_PREFIX" = "" || LDFLAGS_LIBMPI="$LDFLAGS_LIBMPI -L$GM_PREFIX/lib"
   LDLIBS_LIBMPI="$LDLIBS_LIBMPI -lgm"
fi

AC_ARG_ENABLE(ib,
              AC_HELP_STRING([--enable-ib], [enable InfiniBand path]))
echo "feature InfiniBand... ${enable_ib:-no}"
if test "$enable_ib" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_INFINIBAND"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/ib"
   test "$IB_PREFIX" = "" || CPPFLAGS="$CPPFLAGS -I$IB_PREFIX/include"
   test "$IB_PREFIX" = "" || LDFLAGS="$LDFLAGS -L$IB_PREFIX/lib"
   LIBS="$LIBS -Wl,--allow-multiple-definition -Wl,-Bstatic -lvapi -lmosal -lmpga -lmtl_common -Wl,-Bdynamic"
fi

AC_ARG_ENABLE(shared_memory,
              AC_HELP_STRING([--enable-shared-memory], [enable shared memory path (probably required)]))
echo "feature SHARED_MEMORY... ${enable_shared_memory:-no}"
if test "$enable_shared_memory" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_SHARED_MEMORY"
   MODULES_LIBMPI="$MODULES_LIBMPI src/path/sharedmem"
fi

AC_ARG_ENABLE(reliability,
              AC_HELP_STRING([--enable-reliability], [enable reliablity]))
echo "feature RELIABILITY... ${enable_reliability:-no}"
if test "$enable_reliability" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_RELIABILITY"
fi

AC_ARG_ENABLE(ct,
              AC_HELP_STRING([--enable-ct], [enable control topology]))
echo "feature CT... ${enable_ct:-no}"
if test "$enable_ct" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_CT"
fi

AC_ARG_ENABLE(check_api_args,
              AC_HELP_STRING([--enable-check-api-args], [enable checking of API arguments]))
echo "feature CHECK_API_ARGS... ${enable_check_api_args:-no}"
if test "$enable_check_api_args" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_CHECK_API_ARGS"
fi

AC_ARG_ENABLE(enable-dbg,
              AC_HELP_STRING([--enable-dbg], [enable copious debugging output]))
echo "copious debugging output... ${enable_dbg:-no}"
if test "$enable_dbg" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_DBG"
fi

AC_ARG_ENABLE(memprofile,
              AC_HELP_STRING([--enable-memprofile], [enable memory profiling]))
echo "feature MEMPROFILE... ${enable_memprofile:-no}"
if test "$enable_memprofile" = "yes" ; then
   CPPFLAGS="$CPPFLAGS -DENABLE_MEMPROFILE"
fi

AC_ARG_WITH(romio,
            AC_HELP_STRING([--with-romio], [include MPI-IO support]))
echo "package ROMIO... ${with_romio:-no}"

AC_MSG_CHECKING([makefile configuration include])

# output

AC_SUBST(F90)
AC_SUBST(LDFLAGS_LIBMPI)
AC_SUBST(LDFLAGS_LIBMPI)
AC_SUBST(LDFLAGS_MPIRUN)
AC_SUBST(LDLIBS_LIBMPI)
AC_SUBST(LDLIBS_MPIRUN)
AC_SUBST(LN_S)
AC_SUBST(MODULES_LIBMPI)
AC_SUBST(MODULES_MPIRUN)
AC_SUBST(MODULES_OS)
AC_SUBST(EXEEXT)
AC_SUBST(OBJEXT)
AC_SUBST(LIBEXT)
AC_SUBST(DLLEXT)
AC_SUBST(enable_debug)
AC_SUBST(enable_dynamic)
AC_SUBST(enable_static)
AC_SUBST(with_romio)
AC_SUBST(romiodir)
AC_SUBST(ROMIO_FS)

AC_OUTPUT([Makefile])
