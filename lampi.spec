%define version 1.4.X
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
The Los Alamos Message Passing Interface (LA-MPI) project provides an
end-to-end network fault-tolerant message passing system for
tera-scale clusters. Our objective is to deliver to the application
developer a production quality open-source software implementation of
a message passing library with leading edge technology underpinnings.
The current API supported being MPI 1.2 (C and Fortran bindings).

%prep

%setup

%build

%configure
%{__make}

%install

%{__rm} -rf %{buildroot}
%makeinstall

# rm -f /usr/man (useless symlink)
%{__rm} -f %{buildroot}/man

# populate /usr/share/doc/lampi-1.3.X/ by hand
%define mydocdir /usr/share/doc/%{name}-%{version}
%define mydocroot %{buildroot}%{mydocdir}
%{__mkdir} -p %{mydocroot}
%{__install} COPYRIGHT INSTALL LICENSE README README.RPM %{mydocroot}
%{__install} %{buildroot}%{_datadir}/lampi/* %{mydocroot}
%{__rm} -rf %{buildroot}%{_datadir}/lampi

%clean

%{__rm} -rf %{buildroot}

%files

%defattr(-,root,root)
%{_bindir}
%{_includedir}
%{_libdir}
%doc %{mydocdir}
%doc %{_datadir}/man
%config /etc

%changelog

* Wed Jun 18 2003 Jason R. Mastaler <jasonrm@lanl.gov>
  - LA-MPI now uses autoconf.

* Thu Apr 10 2003 Jason R. Mastaler <jasonrm@lanl.gov>
  - First cut.
