%define version 1.2.0
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

%{__make}

%install

%{__rm} -rf %{buildroot}
%makeinstall

# mv /usr/etc/* /etc
%{__mkdir} %{buildroot}%{_sysconfdir}
%{__install} %{buildroot}/usr/etc/* %{buildroot}%{_sysconfdir}
%{__rm} -rf %{buildroot}/usr/etc

# rm -f /usr/man
%{__rm} -f %{buildroot}/man

# populate /usr/share/doc/lampi-1.2.0/ by hand
%define mydocdir /usr/share/doc/%{name}-%{version}
%define mydocroot %{buildroot}%{mydocdir}
%{__mkdir} -p %{mydocroot}
%{__install} COPYRIGHT INSTALL LICENSE README %{mydocroot}
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

* Thu Apr 10 2003 Jason R. Mastaler <jasonrm@lanl.gov>
  - First cut.