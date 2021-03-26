# Fallout (MSBDS) Proof-of-Concept

This repository contains Proof-of-Concept exploits for vulnerabilities described in [**Fallout: Leaking Data on Meltdown-resistant CPUs**](https://mdsattacks.com/files/fallout.pdf) by Canella, Genkin, Giner, Gruss, Lipp, Minkin, Moghimi, Piessens, Schwarz, Sunar, Van Bulck and Yarom.
For in-depth info about how these exploits work, please refer to the paper.

All demos were tested on an Intel Core i7-8550U CPU with Debian and Linux Kernel 5.10.0.

## Setup

To successfully run these exploits, an x86_64 Intel CPU and a fairly modern Linux-based OS are required.
While VMs will likely work, the demos will not run on Windows or any other OS directly. For best results, restrict the execution to a single CPU
core with _taskset_ or _numactl_. We also recommend using a CPU that supports Intel TSX, but this is not strictly required.  

You will need root permissions for some demos.

**Dependencies**

You will need _cmake_ to build the demos. Since this repository also contains a kernel-module, you should also install the Linux Headers for your Kernel version.  
Example for Debian and Ubuntu:
<!-- prettier-ignore -->
```shell
 sudo apt-get install cmake linux-headers-$(uname -r) 
 ```
**Build**

Building is as simple a cloning the repository and running
```shell
 make
 ```
If your CPU supports Intel TSX, you can manually disable TSX support with 
```shell
 make notsx
 ```

## Demo #1: Write Transient Forwarding

```shell
taskset 0x1 ./demo_user_read
 ```
This demonstration contains a toylike example of Write Transient Forwarding (WTF),
which is very similar to the example presented in Section 3 of the paper. It will attempt 
to perform a number of reads from random page offsets. On a vulnerable system, the success 
rate of those reads should be around 90%. A success rate of <1% indicates that your system is
likely invulnerable in its current configuration.

## Demo #2: Observing kernel writes

Work in progress...

## Demo #3: Data Bounce

```shell
taskset 0x1 ./demo_data_bounce
 ```

Data bounce is a means to determine if a virtual address is backed by a physical memory page.
This demo checks if data bounce is possible on your system. If this is the case, the rate of true positives should be close to 100%, the rate of false positives should be at 0%. 

## Demo #4: Breaking KASLR

```shell
taskset 0x1 ./demo_kaslr_noroot
 ```
or
```shell
sudo taskset 0x1 ./demo_kaslr
 ```


This demonstration uses data bounces to detect which kernel pages are mapped, effectively breaking KASLR.
```sudo taskset 0x1 ./demo_kaslr``` additionally compares its findings with the correct base address, which is obtained from _/proc/kallsyms_.


 ## Warnings

* We provide this code as-is, without any warranty of any kind. It may cause undesirable behaviour on your system, 
  or not work at all. You are fully responsible for any consequences that might arise from running this code. Please refer to the license for further information.
  
  


* This code is meant for testing purposes and should not be used on production systems. _Do not run this code on any machine that is not yours, or could be used by another person._
 

## Acknowledgements

Parts of this project are based on the [Meltdown Proof-of-Concept](https://github.com/IAIK/meltdown).
