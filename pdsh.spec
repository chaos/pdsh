Name: 
Version: 
Release: 
Summary: Parallel remote shell program.
Copyright: none
Group: System Environment/Base
Source: %{name}-%{version}-%{release}.tgz
BuildRoot: /var/tmp/%{name}-buildroot
Prereq: genders

%description
Pdsh is a reimplementation of the dsh command supplied with the IBM PSSP
product that uses POSIX threads within a single process, and achieves better 
performance while using fewer system resources (in particular, privileged 
sockets and process slots).

%prep
%setup -n %{name}-%{version}-%{release}

%build
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
install -s pdsh $RPM_BUILD_ROOT/usr/bin/pdcp
install -s pdsh $RPM_BUILD_ROOT/usr/bin/pdsh
cp dshbak $RPM_BUILD_ROOT/usr/bin/
gzip pdsh.1 pdcp.1 dshbak.1
cp pdsh.1.gz $RPM_BUILD_ROOT/usr/man/man1
cp pdcp.1.gz $RPM_BUILD_ROOT/usr/man/man1
cp dshbak.1.gz $RPM_BUILD_ROOT/usr/man/man1

%files
%defattr(-,root,root)
%doc README ChangeLog DISCLAIMER README.KRB4
%attr(4755, root, root) /usr/bin/pdsh
%attr(4755, root, root) /usr/bin/pdcp 
/usr/bin/dshbak
/usr/man/man1/pdsh.1.gz
/usr/man/man1/pdcp.1.gz
/usr/man/man1/dshbak.1.gz
