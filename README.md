# Fallout Proof-of-Concept

This repository contains Proof-of-Concept exploits for vulnerabilities described in [**Fallout: Leaking Data on Meltdown-resistant CPUs**](https://mdsattacks.com/files/fallout.pdf) by Canella, Genkin, Giner, Gruss, Lipp, Minkin, Moghimi, Piessens, Schwarz, Sunar, Van Bulck and Yarom.
For in-depth info about how these exploits work, please refer to the paper.

All demos were tested on an Intel Core i7-8550U CPU with Debian and Linux Kernel 5.10.0. For some demos, KPTI had to be manually disabled. 

## Setup

To successfully run these exploits, an x86_64 or i386 capable Intel CPU and a fairly modern Linux-based OS are required.
While VMs will likely work, the demos will not run on Windows or any other OS directly. For best results, restrict the execution to a single CPU
core with _taskset_ or _numactl_. We also recommend use a CPU that is capable of Intel TSX, but this is not strictly required.  

You will need root permissions for some demos.

**Dependencies**

You will need _cmake_ to build the demos. Since this repository also contains a kernel-module, you should also install the Linux Headers for your Kernel version.  
Example for Debian and Ubuntu:
<!-- prettier-ignore -->
```shell
 sudo apt-get install cmake linux-headers-$(uname -r) 
 ```
**Build**

Building is as simple a cloning the repository, entering its directory in the terminal and typing
```shell
 make
 ```
If your CPU supports Intel TSX, but you still want to run the demos without TSX support, you can build them with
```shell
 make notsx
 ```
Note that running the demos without TSX support sometimes yields better results.

## Demo #1: Write Transient Forwarding

```shell
taskset 0x1 ./demo_user_read1
 ```
or 
```shell
taskset 0x1 ./demo_user_read2
 ```
This demonstration contains a toylike example of Write Transient Forwarding (WTF),
which is very similar to the example presented in Section 3 of the paper. It will attempt 
to perform a number of reads from random page offsets. On a vulnerable system, the success 
rate of those reads should be around 90%. A success rate of <1% indicates that your system is
likely invulnerable in its current configuration.

The difference between ```./demo_user_read1``` and ```./demo_user_read2```is that ```./demo_user_read1```
observes writes to the memory heap, while ```./demo_user_read2``` observes writes to the memory stack. 

## Demo #2: Observing kernel writes

Work in progress...

## Demo #3: Data Bounces

```shell
taskset 0x1 ./demo_data_bounce
 ```

Data bounces are a means to determine if a virtual address is backed by a physical memory page.
This demo checks if data bounces are possible on your system. If this is the case, the success rate should be close to 100%. 

## Demo #4: Breaking KASLR

Note: This demo will only work if KPTI is not enabled on your system.

```shell
taskset 0x1 ./demo_kaslr_noroot
 ```
or
```shell
sudo taskset 0x1 ./demo_kaslr
 ```


This demonstration uses data bounces to detect which kernel pages are mapped, effectively breaking KASLR.
```sudo taskset 0x1 ./demo_kaslr``` additionally compares its findings with the actual base address, which is obtained from /proc/kallsyms.

