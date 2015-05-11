
---

## Overview ##

Pdsh is a high-performance, parallel remote shell utility. It uses a sliding
window of threads to execute remote commands, conserving socket resources
while allowing some connections to timeout if needed. It was originally written
as a replacement for IBM's DSH on clusters at LLNL.

The core functionality of pdsh is supplemented by many available dynamically
loadable modules. The modules may implement a new connection protocol,
target host filtering (e.g. removing hosts that are "down" from the target host
list) and/or other host selection options (e.g. _-a_ selects all hosts from
a config file).

The **pdsh** distribution also contains a parallel remote copy utility (**pdcp** -
copy from local host to a group of remote hosts in parallel),
reverse parallel remote copy (**rpdcp**, copy from a group of hosts to localhost in parallel), and a script **dshbak** for formatting and demultiplexing **pdsh** output.


---

## Other Information ##

  * Check out the latest [NEWS](http://pdsh.googlecode.com/git/NEWS)
  * Read about basic [PDSH Usage](UsingPDSH.md)
  * Information about [PDSH Modules](MiscModules.md)
  * [RCMD Module](RCMDModules.md) information