Summary: Parallel remote shell program.
Name: pdsh
Version: 1.4
Release: 1
Copyright: none
Group: System Environment/Base
Source: pdsh-1.4.tgz
BuildRoot: /var/tmp/%{name}-buildroot

%description
Pdsh is a reimplementation of the dsh command supplied with the IBM PSSP
product that uses POSIX threads within a single process, and achieves better 
performance while using fewer system resources (in particular, privileged 
sockets and process slots).

%prep
%setup

%build
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
install -s -o root -m 4755 pdsh $RPM_BUILD_ROOT/usr/bin
install -s -o root -m 4755 pdsh $RPM_BUILD_ROOT/usr/bin/pcp
install -m 755 dshbak $RPM_BUILD_ROOT/usr/bin
gzip pdsh.1 pcp.1 dshbak.1
install -m 644 pdsh.1.gz $RPM_BUILD_ROOT/usr/man/man1
install -m 644 pcp.1.gz $RPM_BUILD_ROOT/usr/man/man1
install -m 644 dshbak.1.gz $RPM_BUILD_ROOT/usr/man/man1

%files
%doc README ChangeLog DISCLAIMER README.KRB4

/usr/bin/pdsh
/usr/bin/pcp
/usr/bin/dshbak
/usr/man/man1/pdsh.1.gz
/usr/man/man1/pcp.1.gz
/usr/man/man1/dshbak.1.gz

%changelog
