Name: 
Version: 
Release: 1
Summary: Parallel remote shell program.
Copyright: none
Group: System Environment/Base
Source: %{name}-%{version}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}
Prereq: genders

%description
Pdsh is a reimplementation of the dsh command supplied with the IBM PSSP
product that uses POSIX threads within a single process, and achieves better 
performance while using fewer system resources (in particular, privileged 
sockets and process slots).

%package qshd
Summary: Remote shell daemon for pdsh/Elan3
Group: System Environment/Base
Prereq: xinetd, qswelan-r
%description qshd
Remote shell service for running Quadrics Elan3 jobs under pdsh.
Sets up Elan capabilities and environment variables needed by Quadrics MPICH
executables.

%prep
%setup

%build
make all qshd

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
mkdir -p $RPM_BUILD_ROOT/etc/xinetd.d
install -s pdsh $RPM_BUILD_ROOT/usr/bin/pdcp
install -s pdsh $RPM_BUILD_ROOT/usr/bin/pdsh
install -s qshd $RPM_BUILD_ROOT/usr/sbin/in.qshd
cp dshbak $RPM_BUILD_ROOT/usr/bin/
gzip pdsh.1 pdcp.1 dshbak.1
cp pdsh.1.gz $RPM_BUILD_ROOT/usr/man/man1
cp pdcp.1.gz $RPM_BUILD_ROOT/usr/man/man1
cp dshbak.1.gz $RPM_BUILD_ROOT/usr/man/man1
cp qshell.xinetd $RPM_BUILD_ROOT/etc/xinetd.d/qshell

%files
%defattr(-,root,root)
%doc README ChangeLog DISCLAIMER README.KRB4
%attr(4755, root, root) /usr/bin/pdsh
%attr(4755, root, root) /usr/bin/pdcp 
/usr/bin/dshbak
/usr/man/man1/pdsh.1.gz
/usr/man/man1/pdcp.1.gz
/usr/man/man1/dshbak.1.gz

%files qshd
/usr/sbin/in.qshd
/etc/xinetd.d/qshell

%post qshd
if ! grep "^qshell" /etc/services >/dev/null; then
        echo "qshell   523/tcp  # pdsh/elan3" >>/etc/services
fi
