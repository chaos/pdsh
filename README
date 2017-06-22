+-------------+
| Description |
+-------------+
Pdsh is a multithreaded remote shell client which executes commands on
multiple remote hosts in parallel.  Pdsh can use several different
remote shell services, including standard "rsh", Kerberos IV, and ssh.

See the man page in the doc directory for usage information.

+---------------+
| Configuration |
+---------------+

Pdsh uses GNU autoconf for configuration.  Dynamically loadable
modules of each shell service (as well as other features) will be
compiled based on configuration.  By default, rsh, Kerberos IV, 
and SDR (for IBM SPs) will be compiled if they exist on the system.

The README.modules file distributed with pdsh contains a description
of each module available, as well as its requirements and/or 
conflicts.

If your system does not support dynamically loadable modules, you
may compile modules in statically using the --enable-static-modules
option.

To configure in additional feature modules:

./configure [options]

--without-rsh 
	Disable support for BSD rcmd(3) (standard rsh).

--with-ssh
        Enable support of ssh(1) remote shell service.

--with-machines=/path/to/machines
	Use a flat file list of machine names for -a instead of genders

--with-genders
        Enable support of a genders database through the genders(3)
        library.  For pdsh's -i option to function properly, the genders
        database must have alternate node names listed as the value of
        the "altname" attribute. 

--with-dshgroups
		Enable support of dsh-style group files in ~/.dsh/group/groupname
		or /etc/dsh/group/groupname. Allows use of -g/-X to target
		or exclude hosts in dsh group files.

--with-netgroup
		Enable use of netgroups (via /etc/netgroup or NIS) to build lists
		of target hosts using -g/-X to include/exclude hosts.

--with-nodeupdown
        Enable support of dynamic elimination of down nodes through
        the nodeupdown(3) library. 

--with-mrsh
        Enable support of mrsh(1) remote shell service.
   
--with-slurm
	Support running pdsh under SLURM allocation.

--with-fanout=N
	Specify default fanout (default is 32).

--with-timeout=N
	Set default connect timeout (default is 10 seconds).

--with-readline
	Use the GNU readline library to parse input in interactive mode.

Note that a number of the above configurations options may "conflict"
with each other because they perform identical operations.  For
example, genders and nodeattr both support the -g option.  If several
modules are installed that support identical options, the options will
default to one particular module.  Static compilation of modules will
fail if conflicting modules are selected.  See the man page in this
directory for details on which modules conflict.

+------------+
| INSTALLING |
+------------+
make
make install

By default, pdsh is now installed without setuid permissions. This
is because, for the majority of the rcmd connect protocols, root
permissions are not necessarily needed. If you are using either of
the "rcmd/rsh" or "rcmd/qsh" modules, you will need to change the
permissions of pdsh and pdcp to be setuid root after the install.
For example:

 > chown root PREFIX/bin/pdsh PREFIX/bin/pdcp
 > chmod 4755 PREFIX/bin/pdsh PREFIX/bin/pdcp

+---------+
| GOTCHAS |
+---------+

Watch out for the following gotchas:

1) When executing remote commands via rsh, krb4, qsh, or ssh, pdsh
uses one reserved socket for each active connection, two if it is
maintaining a separate connection for stderr.  It obtains these
sockets by calling rresvport(), which normally draws from a pool of
256 sockets.  You may exhaust these if multiple pdsh's are running
simultanously on a machine, or if the fanout is set too high.  Mrsh 
and mqsh do not use reserved ports, and therefore are not affected
this problem as severely. 

2) When pdsh is using a remote shell service that is wrapped with TCP
wrappers, there are three areas where bottlenecks can be created:
IDENT, DNS, and SYSLOG.  If your hosts.allow includes "user@", e.g.
"in.rshd : ALL@ALL : ALLOW" and TCP wrappers is configured to support
IDENT, each simultaneous remote shell connection will result in an
IDENT query back to the source.  For large fanouts this can quickly
overwhelm the source.  Similarly, if TCP wrappers is configured to
query the DNS on every connection, pdsh may overwhelm the DNS server.
Finally, if every remote shell connection results in a remote syslog
entry, syslogd on your loghost may be overwhelmed and logs may grow
excessively long.

If local security policy permits, consider configuring TCP wrappers to
avoid calling IDENT, DNS, or SYSLOG on every remote shell connection.
Configuring without the "PARANOID" option (which requires all
connections to be registered in the DNS), permitting a simple list of
IP addresses or a subnet (no names, and no user@ prefix), and setting
the SYSLOG severity for the remote shell service to a level that is
not remotely logged will avoid these pitfalls.  If these actions are
not possible, you may wish to reduce pdsh's default fanout (configure
--with-fanout=N).

+---------------------+
| THEORY OF OPERATION |
+---------------------+
We will generalize for the common remote shell service rsh.  The
following is similar for all other shell services (ssh, krb4, qsh,
etc.), but other shell services may include additional security or
features.

A thread is created for each rsh connection to a node.  Each thread
opens a connection using an MT-safe rcmd-like function, returns
stdin and stderr streams, then terminates.

The mainline starts fanout number of rsh threads and waits on a
condition variable that is signalled by the rsh threads as they
terminate.  When the condition variable is signalled, the main thread
starts a new rsh thread to maintain the fanout, until all remote
commands have been executed.

A timeout thread is created that monitors the state of the threads and
terminates any that take too much time connecting or, if requested on
the command line, take too long to complete.

Typing ^C causes pdsh to list threads that are in the connected state.
Another ^C immediately following the first one terminates the program.

+--------+
| AUTHOR |
+--------+
Jim Garlick <garlick@llnl.gov>

Please send suggestions, bug reports, or just a note letting me know
that you are using pdsh (it would be interesting to hear how many
nodes are in your cluster).

+------+
| NOTE |
+------+
This product includes software developed by the University of
California, Berkeley and its contributors.  Modifications have been
made and bugs are probably mine.

The PDSH software package has no affiliation with the Democratic Party
of Albania (www.pdsh.org).
