# rpmbuild --with root=<prefix>
%if %{?_with_root:1}%{!?_with_root:0}
%global	rootdir		%(perl -e '$_ = "%{_with_root}"; s/^.*--with-root=(\\S+)$/$1/; print;')
%else
%global	rootdir		/usr
%endif

%if %{?_with_gm:1}%{!?_with_gm:0}
%global dev		gm
%else
%global dev		p4
%endif

%global	retcode		0

# rpmbuild --with absoft
%if %{?_with_absoft:1}%{!?_with_absoft:0}
%global retcode		%{?compiler:1}%{!?compiler:0}
%global	compiler	absoft-
%endif

# rpmbuild --with intel
%if %{?_with_intel:1}%{!?_with_intel:0}
%global retcode		%{?compiler:1}%{!?compiler:0}
%global	compiler	intel-
%endif

# rpmbuild --with lahey
%if %{?_with_lahey:1}%{!?_with_lahey:0}
%global retcode		%{?compiler:1}%{!?compiler:0}
%global	compiler	lahey-
%endif

# rpmbuild --with nag
%if %{?_with_nag:1}%{!?_with_nag:0}
%global retcode		%{?compiler:1}%{!?compiler:0}
%global	compiler	nag-
%endif

# rpmbuild --with pgi
%if %{?_with_pgi:1}%{!?_with_pgi:0}
%global retcode		%{?compiler:1}%{!?compiler:0}
%global	compiler	pgi-
%endif

# default
%if %{!?compiler:1}%{?compiler:0}
%define	compiler	%{nil}
%endif

%define version 1.4.5
%define name lampi
%define release 1

Summary: Los Alamos Message Passing Interface
Name: %{name}
Version: %{version}
Release: %{release}
Source0: http://www.acl.lanl.gov/la-mpi/lampi-papers/%{name}-%{version}.tar.gz
License: LGPL
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Vendor: Los Alamos National Laboratory
Packager: lampi-support@lanl.gov
URL: http://www.acl.lanl.gov/la-mpi/
Provides: mpi

%description 
The Los Alamos Message Passing Interface is an open-source, portable
implementation of MPI designed for large clusters of multiprocessor nodes.
LA-MPI offers high performance, network fault tolerance, thread safety,
and implements MPI version 1.2 with MPI-IO supported via ROMIO.

%package %{compiler}%{dev}
Summary: LA-MPI - %{dev} device
Group: System Environment/Libraries

%description  %{compiler}%{dev}
The Los Alamos Message Passing Interface is an open-source, portable
implementation of MPI designed for large clusters of multiprocessor nodes.
LA-MPI offers high performance, network fault tolerance, thread safety,
and implements MPI version 1.2 with MPI-IO supported via ROMIO.

%prep

%setup

%build

%if "1" == "%{retcode}"
%{error:--with: too many compilers specified!}
exit 1
%endif

# rpmbuild --with absoft
%if %{?_with_absoft:1}%{!?_with_absoft:0}
if [ -z "$CC" ];	then CC='gcc' ; fi
if [ -z "$CXX" ];	then CXX='g++' ; fi
if [ -z "$FC" ];	then FC='f77' ; fi
if [ -z "$F77" ];	then F77='f77' ; fi
if [ -z "$FFLAGS" ];	then FFLAGS='-f -N15' ; fi
if [ -z "$F90" ];	then F90='f90' ; fi
if [ -z "$F90FLAGS" ];	then F90FLAGS='-YEXT_NAMES=LCS -YEXT_SFX=_' ; fi
%endif

# rpmbuild --with intel
%if %{?_with_intel:1}%{!?_with_intel:0}
if [ -z "$CC" ];	then CC='icc' ; fi
if [ -z "$CXX" ];	then CXX='icc' ; fi
if [ -z "$FC" ];	then FC='ifc' ; fi
if [ -z "$F77" ];	then F77='ifc' ; fi
if [ -z "$F90" ];	then F90='ifc' ; fi
%endif

# rpmbuild --with lahey
%if %{?_with_lahey:1}%{!?_with_lahey:0}
if [ -z "$CC" ];	then CC='gcc' ; fi
if [ -z "$CXX" ];	then CXX='g++' ; fi
if [ -z "$FC" ];	then FC='lf95' ; fi
if [ -z "$F77" ];	then F77='lf95' ; fi
if [ -z "$F90" ];	then F90='lf95' ; fi
%endif

# rpmbuild --with nag
%if %{?_with_nag:1}%{!?_with_nag:0}
if [ -z "$CC" ];	then CC='gcc' ; fi
if [ -z "$CXX" ];	then CXX='g++' ; fi
if [ -z "$FC" ];	then FC='f95' ; fi
if [ -z "$F77" ];	then F77='f95' ; fi
if [ -z "$F90" ];	then F90='f95' ; fi
%endif

# rpmbuild --with pgi
%if %{?_with_pgi:1}%{!?_with_pgi:0}
if [ -z "$CC" ];	then CC='pgcc' ; fi
if [ -z "$CFLAGS" ];	then CFLAGS='-g77libs -B' ; fi
if [ -z "$CXX" ];	then CXX='pgCC' ; fi
if [ -z "$CXXFLAGS" ];	then CXXFLAGS='-g77libs' ; fi
if [ -z "$FC" ];	then FC='pgf77' ; fi
if [ -z "$F77" ];	then F77='pgf77' ; fi
if [ -z "$FFLAGS" ];	then FFLAGS='-g77libs' ; fi
if [ -z "$F90" ];	then F90='pgf90' ; fi
if [ -z "$F90FLAGS" ];	then F90FLAGS='-g77libs' ; fi
%endif

# default
%if %{!?compiler:1}%{?compiler:0}
if [ -z "$CC" ];	then CC='gcc' ; fi
if [ -z "$CXX" ];	then CXX='g++' ; fi
if [ -z "$FC" ];	then FC='g77' ; fi
if [ -z "$F77" ];	then F77='g77' ; fi
%endif

export PATH CC CFLAGS CXX CXXFLAGS FC F77 FFLAGS F90 F90FLAGS

%global _prefix		%{rootdir}/lampi-%{compiler}%{version}/%{dev}

./configure		--prefix=%{_prefix} \
%{?_with_debug:		--enable-debug}		%{?_without_debug:	--disable-debug} \
%{?_with_static:	--enable-static} 	%{?_without_static	--disable-static} \
%{?_with_dynamic:	--enable-dynamic}	%{?_without_dynamic:	--disable-dynamic} \
%{?_with_lsf:		--enable-lsf}		%{?_without_lsf:	--disable-lsf} \
%{?_with_rms:		--enable-rms}		%{?_without_rms:	--disable-rms} \
%{?_with_bproc:		--enable-bproc}		%{?_without_bproc:	--disable-bproc} \
%{?_with_tcp:		--enable-tcp}		%{?_without_tcp:	--disable-tcp} \
%{?_with_udp:		--enable-udp}		%{?_without_udp:	--disable-udp} \
%{?_with_qsnet:		--enable-qsnet}		%{?_without_qsnet:	--disable-qsnet} \
%{?_with_gm:		--enable-gm}		%{?_without_gm:		--disable-gm} \
%{?_with_ib:		--enable-ib}		%{?_without_ib:		--disable-ib} \
%{?_with_shared-memory:	--enable-shared-memory}	%{?_without_shared-memory:--disable-shared-memory} \
%{?_with_reliability:	--enable-reliability}	%{?_without_reliability:--disable-reliability} \
%{?_with_ct:		--enable-ct}		%{?_without_ct:		--disable-ct} \
%{?_with_check-api-args:--enable-check-api-args}%{?_without_check-api-args:--disable-check-api-args} \
%{?_with_dbg:		--enable-dbg}		%{?_without_dbg:	--disable-dbg} \
%{?_with_memprofile:	--enable-memprofile}	%{?_without_memprofile:	--disable-memprofile} \
%{?_with_romio:		--with-romio}		%{?_without_romio:	--without-romio} \

%{__make}

%install

%{__rm} -rf %{buildroot}
%{__make} prefix=%{buildroot}%{_prefix} install

# rm -f /usr/man (useless symlink)
%{__rm} -f %{buildroot}/man

# populate /usr/share/doc/lampi-1.3.X/ by hand
%define mydocdir %{_datadir}/doc/%{name}-%{version}
%define mydocroot %{buildroot}%{mydocdir}
%{__mkdir} -p %{mydocroot}
%{__install} COPYRIGHT INSTALL LICENSE README README.RPM %{mydocroot}
%{__install} %{buildroot}%{_datadir}/lampi/* %{mydocroot}
%{__rm} -rf %{buildroot}%{_datadir}/lampi

%clean

%{__rm} -rf %{buildroot}

%files %{compiler}%{dev}

%defattr(-,root,root)
%{_bindir}
%{_includedir}
%{_libdir}
%doc %{mydocdir}
%doc %{_datadir}/man
%config %{_prefix}/etc

%changelog
* Tue Oct 07 2003 Daryl W. Grunau <dwg@lanl.gov>
  - Enable --with processing.  Note that I've been able to link/run x-compiled codes
    against a GCC-compiled LA-MPI library, so --with <your favorite compiler> may not
    be necessary.

* Wed Jun 18 2003 Jason R. Mastaler <jasonrm@lanl.gov>
  - LA-MPI now uses autoconf.

* Thu Apr 10 2003 Jason R. Mastaler <jasonrm@lanl.gov>
  - First cut.
