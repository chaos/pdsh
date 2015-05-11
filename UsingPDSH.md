#summary General information about using pdsh

# Using PDSH #

  * **[Introduction](#Introduction.md)**
  * **[Targeting remote hosts](#Targeting_remote_hosts.md)**
    * **[Creating the target host list with -w](#Creating_the_target_host_list_with_-w.md)**
    * **[Excluding target hosts with -x](#Excluding_target_hosts_with_-x.md)**
    * **[The WCOLL environment variable](#The_WCOLL_environment_variable.md)**
    * **[Other methods](#Other_methods_to_create_the_target_host_list.md)**
  * **[PDSH Modules](#PDSH_Modules.md)**
  * **[Other Standard PDSH options](#Other_Standard_PDSH_options.md)**


---

## Introduction ##
Pdsh is a parallel remote shell client. Basic usage is via the familiar syntax

> <b><code>pdsh</code></b> _`[OPTION]`_... _COMMAND_

where _COMMAND_ is the remote command to run.

Pdsh is a threaded application that uses a sliding window
(or _fanout_) of threads to conserve resources on the initiating
host and allow some connections to time out while all other
connections continue.

Output from each host is displayed as it is recieved and is prefixed with the name
of the host and a '`:`' character, unless the _-N_ option is used.
```
pdsh -av 'grep . /proc/sys/kernel/ostype'
ehype78: Linux
ehype79: Linux
ehype76: Linux
ehype85: Linux
ehype77: Linux
ehype84: Linux
...
```

When **_pdsh_** receives a Ctrl-C (SIGINT), it will list the status
of all current threads

```
[Ctrl-C]
pdsh@hype137: interrupt (one more within 1 sec to abort)
pdsh@hype137:  (^Z within 1 sec to cancel pending threads)
pdsh@hype137: hype0: connecting
pdsh@hype137: hype1: command in progress
pdsh@hype137: hype2: command in progress
pdsh@hype137: hype3: connecting
pdsh@hype137: hype4: connecting
...
```

Another Ctrl-C within one second will cause **_pdsh_** to abort
immediately, while a Ctrl-Z within a second will cancel all
pending threads, allowing threads that are connecting and
"in progress" to complete normally.

```
[Ctrl-C Ctrl-Z]
pdsh@hype137: interrupt (one more within 1 sec to abort)
pdsh@hype137:  (^Z within 1 sec to cancel pending threads)
pdsh@hype137: hype0: connecting
pdsh@hype137: hype1: command in progress
pdsh@hype137: hype2: command in progress
pdsh@hype137: hype3: connecting
pdsh@hype137: hype4: connecting
pdsh@hype137: hype5: connecting
pdsh@hype137: hype6: connecting
pdsh@hype137: Canceled 8 pending threads.
```


---

## Targeting remote hosts ##

At a minimum **_pdsh_** requires a list of remote hosts to target and a remote
command. The standard options used to set the target hosts in **_pdsh_** are
_-w_ and _-x_, which set and exclude hosts respectively.


---

#### Creating the target host list with **_-w_** ####

The _-w_ option is used to set and/or filter the list of target hosts, and
is used as

> ` -w TARGETS... `

where _TARGETS_ is a comma-separated list of the one or more of the following:

  * Normal host names, e.g. ` -w host0,host1,host2... `

  * A single '`-`' character, in which case the list of hosts will be read on STDIN.

  * A range of hosts specified in **HOSTLIST** format. The hostlist format is an optimization for clusters of hosts that contain a numeric suffix. The range of target hosts is specified in brackets after the hostname prefix, e.g
```
     host[0-10] -> host0,host1,host2,...host10
     host[0-2,10] -> host0,host1,host2,host10
```
> See the [HOSTLIST expressions](HostListExpressions.md) page for details on the **HOSTLIST** format.

  * If any argument is preceded by a single '`^`' character, then the argument is taken to be the path to a file containing a list of hosts, one per line.
```
Read hosts from /tmp/hosts    
 pdsh -w ^/tmp/hosts ... 

Also works for multiple files:
 pdsh -w ^/tmp/hosts,^/tmp/morehosts ...
```

  * If the item begins with a '`/'` charcter, then it is a regular expression on which to filter the list of hosts. (The regex argument may be optionally followed by a trailing '`/`', e.g. '`/node.*/`').
```
Select only hosts ending in a 0 via regex:
 pdsh -w host[0-20],/0$/ ...
```

  * If any host, hostlist, filename, or regex item is preceeded by a '`-`' character, then these hosts are excluded instead of including them.
```
Run on all hosts (-a) except host0:
 pdsh -a -w -host0 ...

Exclude all hosts ending in 0:
 pdsh -a -w -/0$/ ...

Exclude hosts in file /tmp/hosts:
 pdsh -a -w -^/tmp/hosts ...
```

Additionally, a list of hosts preceeded by "user@" specifies a
remote username other than the default for these hosts.  , and
list of hosts preceeded by "rcmd\_type:" specifies an alternate
rcmd connect type for the following hosts. If used together, the
rcmd type must be specified first, e.g. `ssh:user1@host0` would use
`ssh` to connect to `host0` as `user1`.

```
Run with user `foo' on hosts h0,h1,h2, and user `bar' on hosts h3,h5:
 pdsh -w foo@h[0-2],bar@h[3,5] ...

Use ssh and user "u1" for hosts h[0-2]:
 pdsh -w ssh:u1@h[0-2] ...
```

> Note: If using the **genders** module, the rcmd\_type for groups
> of hosts can be encoded in the genders file using the
> special **_`pdsh_rcmd_type`_** attribute


---

#### Excluding target hosts with _-x_ ####

The _-x_ option is used to exclude specific hosts from the target
node list and is used simply as
> ` -x TARGETS...`

This option may be used with other node selection
options such as _-a_ and _-g_ (when available). Arguements to
_-x_ may also be preceeded by the filename ('^') and regex ('/')
characters as described above. Also as with _-w_, the _-x_ option
also operates on [HOSTLISTS](HostlistExpresssions.md).

```
Exclude hosts ending in 0:
 pdsh -a -x /0$/ ...

Exclude hosts in file /tmp/hosts:
 pdsh -a -x ^/tmp/hosts ...

Run on hosts node1-node100, excluding node50:
  pdsh -w node[1-100] -x node50 ...

```


---

#### The WCOLL environment variable ####

As an alternative to "_-w ^file_", and for backwards compatibility
with DSH, A file containing a list of hosts to target may also be
specified in the `WCOLL` environment variable, in which case **_pdsh_**
behaves just as if it were called as
> ` pdsh -w ^$WCOLL ...`

For example, to run **_pdsh_** multiple times on the same list of hosts
in `/tmp/hosts`:

```
export WCOLL=/tmp/hosts
pdsh hostname
pdsh date
pdsh ...
```


---

#### Other methods to create the target host list ####

Additionally, there are many other **_pdsh_** modules that provide
options for targetting remote hosts. These are documented in the
[Miscellaneous Modules](MiscModules.md) page, but examples include _-a_
to target all hosts in a machines, genders, or dshgroups file, _-g_
to target groups of hosts with genders, dshgroups, and netgroups,
and _-j_ to target hosts in SLURM or Torque/PBS jobs.


---

## PDSH Modules ##

As described earlier, **_pdsh_** uses modules to implement and extend
its core functionality. There are two basic kinds of modules used
in **_pdsh_** -- "_rcmd_" modules, which implement the remote connection
method pdsh uses to run commands, and "_misc_" modules, which implement
various other **_pdsh_** functionality, such as node list generation
and filtering.

The current list of loaded modules is printed with the `pdsh -V`
output

```
   pdsh -V
   pdsh-2.23 (+debug)
   rcmd modules: ssh,rsh,mrsh,exec (default: mrsh)
   misc modules: slurm,dshgroup,nodeupdown (*conflicting: genders)
   [* To force-load a conflicting module, use the -M <name> option]
```

Note that some modules may be listed as `conflicting` with others.
This is because these modules may provide additional command line options to _pdsh_, and if the command line options conflict, the
options to **_pdsh_**, and if the command line options conflict, the

Detailed information about available modules may be viewed via the
_-L_ option:

```
> pdsh -L
8 modules loaded:

Module: misc/dshgroup
Author: Mark Grondona <mgrondona@llnl.gov>
Descr:  Read list of targets from dsh-style "group" files
Active: yes
Options:
-g groupname      target hosts in dsh group "groupname"
-X groupname      exclude hosts in dsh group "groupname"

Module: rcmd/exec
Author: Mark Grondona <mgrondona@llnl.gov>
Descr:  arbitrary command rcmd connect method
Active: yes

Module: misc/genders
Author: Jim Garlick <garlick@llnl.gov>
Descr:  target nodes using libgenders and genders attributes
Active: no
Options:
-g query,...      target nodes using genders query
-X query,...      exclude nodes using genders query
-F file           use alternate genders file `file'
-i                request alternate or canonical hostnames if applicable
-a                target all nodes except those with "pdsh_all_skip" attribute
-A                target all nodes listed in genders database

...
```

This output shows the module name author, a description, any options
provided by the module and whether the module is currently "active"
or not.

The _-M_ option may be use to force-load a list of modules before
all others, ensuring that they will be active if there is a module
conflict.  In this way, for example, the `genders` module could
be made active and the `dshgroup` module deactivated for one run
of **_pdsh_**. This option may also be set via the `PDSH_MISC_MODULES`
environment variable.


---

## Other Standard PDSH options ##

<dl>
<blockquote><dt> <b><i>-h</i></b> </dt>
<dd> Output a usage message and quit. A list of available rcmd<br>
modules will also be printed at the end of the usage message. The<br>
available options for <b><i>pdsh</i></b> may change based on which modules are<br>
loaded or passed to the <i>-M</i> option. </dd></blockquote>

<blockquote><dt> <b><i>-S</i></b> </dt>
<dd> Return the largest of the remote command return values</dd></blockquote>

<blockquote><dt> <b><i>-b</i></b> </dt>
<dd> Batch mode. Disables the ctrl-C status feature so that a single<br>
ctrl-c kills <b><i>pdsh</i></b>. </dd></blockquote>

<blockquote><dt> <b><i>-l</i></b> <i>USER</i> </dt>
<dd> Run remote commands as user <i>USER</i>. The remote username may<br>
also be specified using the <i>USER@TARGETS</i> syntax with the <i>-w</i>
option </dd></blockquote>

<blockquote><dt> <b><i>-t</i></b> <i>SECONDS</i> </dt>
<dd> Set a connect timeout in seconds. Default is 10. </dd></blockquote>

<blockquote><dt> <b><i>-u</i></b> <i>SECONDS</i> </dt>
<dd> Set a remote command timeout. The default is unlimited. </dd></blockquote>

<blockquote><dt> <b><i>-f</i></b> <i>FANOUT</i> </dt>
<dd> Set the maximum number of simultaneous remote commands to <i>FANOUT</i>.<br>
The default is <i>32</i>, but can be overridden at build time. </dd></blockquote>

<blockquote><dt> <b><i>-N</i></b> </dt>
<dd> Disable the <code>hostname:</code> prefix on lines of <b><i>pdsh</i></b> output.</dd></blockquote>

<blockquote><dt> <b><i>-V</i></b> </dt>
<dd> Output <b><i>pdsh</i></b> version information, along with a list of currently<br>
loaded modules, and exit. </dd>
</dl>
<hr /></blockquote>
