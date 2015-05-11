# PDSH RCMD Modules #

---




---


## Intro ##

The method by which pdsh runs commands on remote hosts may be selected
at runtime using the _-R_ option. This functionality is ultimately
implemented by dynamically loaded modules, and thus the list of available
rcmd modules may differ from installation to installation. A list of
currently available rcmd modules is printed when using any of the
_-h_, _-V_, or _-L_ options. The current default rcmd module will be
displayed with the _-V_ and _-h_ options.


---

## RCMD modules distributed with PDSH ##

> <b><i>rsh</i></b>
> > Uses an internal, thread-safe implementation of BSD _rcmd_(3) to run
> > commands using the standard _rsh_(1) protocol.


> <b><i>exec</i></b>
> > Executes  an  arbitrary command for each target host. The first
> > of the pdsh remote arguments is the local command  to  execute,
> > followed  by  any further arguments. Some simple parameters are
> > substitued on the command line, including  `%h`  for  the  target
> > hostname,  `%u`  for  the  remote username, and `%n` for the remote
> > rank `[0-n]` (To get a literal `%` use `%%`).  For example, the  following
> > would duplicate using the ssh module to run `hostname`(1) across the
> > hosts `foo[0-10]`:
```
  pdsh -R exec -w foo[0-10] ssh -x -l %u %h hostname
```
> > and the following command line would run _`grep`_(1) in parallel across
> > the files `console.foo[0-10]`:
```
   pdsh -R exec -w foo[0-10] grep BUG console.%h
```


> <b><i>ssh</i></b>
> > Uses a variant of _popen_(3) to run multiple copies of the _ssh_(1)
> > command. The **_ssh_** module is actually equivalent to using the **_exec_**
> > module with arguements of "`ssh -x -l %u %h cmd`..." as seen above.
> > This default command line can be overridden via the `PDSH_SSH_ARGS`
> > environment variable, or extra options can be added to the ssh command
> > with the `PDSH_SSH_ARGS_APPEND` environment variable. For example, to
> > direct ssh to port 8022 instead the default port 22, one could use
```
    PDSH_SSH_ARGS_APPEND="-p8022" pdsh -Rssh commands...
```
> > > Note: The `PDSH_SSH_ARGS_APPEND` variable was broken in releases before
> > > pdsh-2.24.


> <b><i>mrsh</i></b>
> > This module uses the _mrsh_(1) protocol to execute jobs on remote hosts.
> > The mrsh protocol uses a credential based authentication, forgoing the need
> > to allocate reserved ports, but otherwise acts just like _rsh_(1). Remote nodes
> > must be running `mrshd`(8) in order for this module to work.


> <b><i>qsh</i></b>
> > Allows pdsh to execute MPI jobs over QsNet.  Qshell  propagates the
> > current working directory, pdsh environment, and Elan capabilities
> > to the remote process. The following environment vari- able   are
> > also  appended to the environment:  `RMS_RANK`, `RMS_NODEID`,
> > `RMS_PROCID`, `RMS_NNODES`, and `RMS_NPROCS`. Since  pdsh needs  to  run
> > setuid root for qshell support, qshell does not directly support
> > propagation of `LD_LIBRARY_PATH` and `LD_PREOPEN`. Instead the
> > `QSHELL_REMOTE_LD_LIBRARY_PATH` and `QSHELL_REMOTE_LD_PREOPEN`
> > environment variables will may be used and  will  be remapped to
> > `LD_LIBRARY_PATH` and `LD_PREOPEN` by the qshell daemon if set.


> <b><i>mqsh</i></b>
> > Similar to qshell, but uses the mrsh protocol instead of the rsh protocol.


> <b><i>xcpu</i></b>
> > The xcpu module uses the xcpu service to execute remote commands.
