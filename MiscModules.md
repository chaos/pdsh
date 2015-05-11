#summary General pdsh module information
# PDSH Modules #

---

## Intro ##

Pdsh modules come in one of two types at this time: **_rcmd_** and
miscellaneous or **_misc_**. The rcmd modules provide remote command
functionality for pdsh, while the _misc_ modules extend the functionality
of pdsh in some other way -- by providing new options to pdsh or creating
or filtering the list of target hosts, for example.

Multiple rcmd modules may be installed at once and are chosen at runtime
by either the _-R type_ option to pdsh, or by setting the environment
variable `PDSH_RCMD_TYPE`. Detailed information about `rcmd` modules is
found in the [RCMD modules](RCMDModules.md) info page.

---


## Module initialization ##

When pdsh initializes it reads all available modules from the
_`pkglibdir`_ (typically `/usr/lib/pdsh` or `/usr/local/lib/pdsh`). If
conflicting modules are installed, they are loaded on a first-come
first-serve basis (i.e. the first module loaded wins). Modules
may be force-initialized by specifying them to the '-M' `pdsh/pdcp`
option, or via the `PDSH_MISC_MODULES` environment variable.

## Getting information about PDSH modules ##

The list of currenlty loaded modules is available in the output of
the _-V_ and _-L_ options. The _-V_ output, which includes the
pdsh version information, gives a terse output that includes conflicting
modules
```
pdsh-2.23 (+debug)
rcmd modules: ssh,rsh,mrsh,exec (default: mrsh)
misc modules: slurm,dshgroup,nodeupdown (*conflicting: genders)
[* To force-load a conflicting module, use the -M <name> option]
```

While _-L_ prints detailed information about each loaded module
including the module name, description, author, whether the module
is active or not, and the list of options the module provides.
For example, for the _genders_ module:
```
Module: misc/genders
Author: Jim Garlick <garl...@llnl.gov>
Descr:  target nodes using libgenders and genders attributes
Active: yes
Options:
-g query,...      target nodes using genders query
-X query,...      exclude nodes using genders query
-F file           use alternate genders file `file'
-i                request alternate or canonical hostnames if applicable
-a                target all nodes except those with "pdsh_all_skip" attribute
-A                target all nodes listed in genders database
```

---


## Misc modules distributed with PDSH ##

> <b>misc/machines</b>
> > The `machines` module provides an implementation of the _-a_
> > option to read a list of _all_ hosts from an `/etc/machines` file.


> <b>misc/genders</b>
> > The genders module provides an interface to libgenders(3) for pdsh,
> > including targetting all hosts in the genders database, groups of hosts
> > based on genders queries, excluding hosts based on genders, and
> > translating hosts from canonical to alternate names (_altname_ in the
> > genders database). The options provided by the `genders` module
> > include:
> > > <b><i>-A</i></b>
> > > > Target all nodes in the genders database. The _-A_ option targets
> > > > all nodes including those with the `pdsh_all_skip` attribute.

> > > <b><i>-a</i></b>
> > > > Target all nodes in the genders database except any with the
> > > > `pdsh_all_skip` attribute. Shorthand for "`-AX pdsh_all_skip`".

> > > <b><i>-g</i></b> _query,..._
> > > > Target nodes based on a series of genders queries, which are
> > > > typically of the form _`attr[=val]`_ where _attr_ is a genders
> > > > attribute with optional value _val_, though genders does support
> > > > a more advanced query interface that supports the union, intersection,
> > > > difference, or complement of genders attributes. See the _nodeattr_(1)
> > > > or _pdsh_(1) manual pages on your system for more information. The
> > > > _-g_ option may also be used as a filter for a target nodelist provided
> > > > by some other option, _-w_ for example:
```
pdsh -w foo[1-10] -g compute ...
```
> > > > would select all nodes with the compute attribute from the hosts
> > > > `foo[1-10]`.

> > > <b><i>-X</i></b> _query,..._
> > > > Exclude nodes that match the specified queries. See documentation of
> > > > _-g_ for information about the form of genders queries.

> > > <b><i>-i</i></b>
> > > > Request translation between canonical and alternate hostnames (i.e.
> > > > from canonical name to the _altname_ in the genders db).

> > > <b><i>-F</i></b> _filename_
> > > > Read genders database from _filename_ instead of the system default.
> > > > If _filename_ does not specify a an absolute path then it is taken
> > > > to be relative to the directory specified by the `PDSH_GENDERS_DIR`
> > > > environment variable (`/etc` by default). An alternate genders file
> > > > may also be specified via the `PDSH_GENDERS_FILE` environment variable.


> <b>misc/nodeupdown</b>
> > This module uses _libnodeupdown_ to filter out a list of known-down
> > hosts using the _-v_ option.


> <b>misc/slurm</b>
> > The slurm module allows pdsh to target nodes based on currently running
> > SLURM jobs. The SLURM module is called after all other node selection
> > options have been processed, and if the target node list is empty,
> > the module will attempt to read a jobid from the `SLURM_JOBID` environment
> > variable (which is set for a running SLURM allocation). If `SLURM_JOBID`
> > references an invalid job, then it will be silently ignored.
> > The _slurm_ module also provides the _-j_ option to explicitly target
> > a running job or jobs, e.g.:
```
pdsh -j 45622,45633 ...
```
> > targets all nodes allocated to the listed jobs, while
```
pdsh -j all ...
```
> > would target any node currently assigned to a running job.


> <b>misc/torque</b>
> > The torque module allows pdsh to target nodes based on currently
> > running Torque/PBS jobs. It behaves very similar to the _slurm_
> > module, but does not support the `-j all` syntax to target all jobs.


> <b>misc/dshgroup</b>
> > The _dshgroup_ module allows pdsh to dsh (or Dancer's shell) style
> > group files from `/etc/dsh/group` or `~/.dsh/group/`. The options
> > provided are
> > <b><i>-g</i></b> _groupname..._
> > > Target nodes in the dsh group file _groupname_ found in either
> > > `~/.dsh/group/groupname` or `/etc/dsh/group/groupname`

> > <b><i>-X</i></b> _groupname..._
> > > Exclude nodes in dsh group file "groupname."


> <b>misc/netgroup</b>
> > The netgroup module allows pdsh to use standard netgroup entries
> > to build lists of target hosts (`/etc/netgroup` or NIS)
> > <b><i>-g</i></b> _groupname..._
> > > Target nodes in netgroup _groupname_.

> > <b><i>-X</i></b> _groupname..._
> > > Exclude nodes in netgroup _groupname_.


---
