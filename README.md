[FastClick](https://www.fastclick.dev)
=========
This is an extended version of the Click Modular Router featuring an
improved Netmap support and a new DPDK support. It was the result of
our ANCS paper available at http://hdl.handle.net/2268/181954, but received
multiple contributions and improvements since then.

The [Wiki](https://github.com/tbarbette/fastclick/wiki) provides documentation about the elements and how to use some FastClick features
such as batching.

Announcements
-------------
Be sure to watch the repository and check out the [GitHub Discussions](https://github.com/tbarbette/fastclick/discussions) to stay up to date!

Quick start for DPDK
--------------------

 * Install DPDK's dependencies (`sudo apt install libelf-dev build-essential pkg-config zlib1g-dev libnuma-dev`)
 * Install DPDK (http://core.dpdk.org/doc/quick-start/). Since 20.11 you have to use meson : `meson build && cd build && ninja && sudo ninja install`
 * Build FastClick, with support for DPDK using the following command:

```
./configure --enable-dpdk --enable-intel-cpu --verbose --enable-select=poll CFLAGS="-O3" CXXFLAGS="-std=c++11 -O3"  --disable-dynamic-linking --enable-poll --enable-bound-port-transfer --enable-local --enable-flow --disable-task-stats --disable-cpu-load
make
```
  * Since DPDK is using Meson and pkg-config, to compile against various, or non-globally installed DPDK versions, one can prepend `PKG_CONFIG_PATH=path/to/libpdpdk.pc/../` to both configure and make.

*You will find more information in the [High-Speed I/O wiki page](https://github.com/tbarbette/fastclick/wiki/High-speed-I-O).*

FastClick "Light"
-----------------
FastClick, like Click comes with a lot of features that you may not use. The following options will improve performance further :
```
./configure --enable-dpdk --enable-intel-cpu --verbose --enable-select=poll CFLAGS="-O3" CXXFLAGS="-std=c++11 -O3"  --disable-dynamic-linking --enable-poll --enable-bound-port-transfer --enable-local --enable-flow --disable-task-stats --disable-cpu-load --enable-dpdk-packet --disable-clone --disable-dpdk-softqueue
make
```
 * Disable task stats suppress statistics tracking for advanced task scheduling with e.g. BalancedThreadSched. With DPDK, it's polling anyway... And as far as scheduling is concerned, RSS++ has a better solution.
 * Disable CPU load will remove load tracking. That is accounting for a CPU percentage while using DPDK by counting cycles spent in empty runs vs all runs. Accessible with the "load" handler.
 * Enable DPDK packet will remove the ability to use any kind of packets other than DPDK ones. The "Packet" class will become a wrapper to a DPDK buffer. You won't be able tu use MMAP'ed packets, Netmap packets, etc. But in general people using DPDK handle DPDK packets. When playing a trace one must copy the data to a DPDK buffer for transmission anyway. This implementation has been improved since the first version and performs better than the default Packet metadata copying **when passing CLEAR false to FromDPDKDevice**. This means you must do the liveness analysis of metadata by yourself (but we'll automate that in the future anyway), as you can't assume an annotation like the VLAN, the timestamp, etc is 0 by default. It would be bad practice in any case to rely on this kind of default value. 
 * Disable clone will remove the indirect buffer copy. So no more reference counting, no more \_data\_packet. But any packet copy like when using Tee will need to completely copy the packet content, just think sequential. That's most pipeline anyway. And you'll loose the ability to process packets in parallel from multiple threads (without copy). But who does that?
 * Disable DPDK softqueue will disable buffering in ToDPDKDevice. Everything it gets will be sent to DPDK, and the NIC right away. When using batching, it's actually not a problem because when the CPU is busy, FromDPDKDevice will take multiple packets at once (limited to BURST), and you'll effectively send batches in ToDPDKDevice. When the CPU is not busy, and you have one packet per batch because there's no more meat then ... well it's not busy so who cares?
 
Ultimately, FastClick will still be impacted by its high flexibility and the many options it supports in each elements. This is adressed by PacketMill by embedding constant parameters, and other stuffs to produce one efficient binary.

Contribution
------------
FastClick also aims at keeping a more up-to-date fork and welcomes
contributions from anyone.

Regular contributors will be given direct access to the repository.
The general rule of thumb to accept a pull request is to involve
two different entities. I.e. someone for company A make a PR and
someone from another company/research unit merges it.

*You will find more information about contributions in the [Community Contributions wiki page](https://github.com/tbarbette/fastclick/wiki/Community-Contributions).*

Examples
--------
See conf/fastclick/README.md
The wiki provides more information about the [I/O frameworks you should use for high speed](https://github.com/tbarbette/fastclick/wiki/High-speed-I-O), such as DPDK and Netmap, and how to configure them.

Differences with the mainline Click (kohler/click)
--------------------------------------------------
In a nutshell:
 - Batching
 - The DPDK version in mainline is very limited (no native multi-queue, you have to duplicate elements etc)
 - Thread vectors, allowing easier thread management
 - The flow subsystem that comes from MiddleClick and allow to use many classification algorithm for new improved NAT, Load Balancers, DPI engine (HyperScan, SSE4 string search), Statistics tracking, etc
 - By defaults FastClick compiles with userlevel multithread. You still have to explicitely *--enable-dpdk* if you want fast I/O with DPDK (you do)

*You will find more information about the differences with Click in the [related wiki page](https://github.com/tbarbette/fastclick/wiki/Differences-between-FastClick-and-Click)*

Differences with the ANCS paper
-------------------------------
For simplicity, we reference all input element as "FromDevice" and output
element as "ToDevice". However in practice our I/O elements are 
FromNetmapDevice/ToNetmapDevice and FromDPDKDevice/ToDPDKDevice. They both
inherit from QueueDevice, which is a generic abstract element to implement a
device which supports multiple queues (or in a more generic way I/O through
multiple different threads).

Thread vector and bit vector designate the same thing.

Getting help
------------
Use the github issue tracker (https://github.com/tbarbette/fastclick/issues) or
contact barbette at kth.se if you encounter any problem.

Please do not ask FastClick-related problems on the vanilla Click mailing list.
If you are sure that your problem is Click related, post it on vanilla Click's
issue tracker (https://github.com/kohler/click/issues).

The original Click readme is available in the README.original file.
