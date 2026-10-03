
# poclmembench
## Portable OpenCL Memory Benchmark 

**poclmemebench** benchmarks amd gpu memory, works on platforms:

*	Linux
*	MacOS
*	Windows

Project inspired by delphi OclMemBench by duzenko [https://github.com/duzenko/OpenclMemBench](https://github.com/duzenko/OpenclMemBench "github")  


## Install

Standalone **executables** are provided in the
[Releases](https://github.com/Lyky35/poclmembench/releases) section:

| Archive | Platform |
| ------- | -------- |
| `poclmembench_lin_0_92.tar.gz` | Linux |
| `poclmembench_win_0_92.zip` | Windows |

Download an archive for your operating system and unpack the content to a place
accessible from command line, or build it from source for other platforms
(see **Build** below).

## Usage

The **poclmembench** is a command line utility. This means you launch it either
from a Windows command prompt or Linux console, or create shortcuts to
predefined command lines using a Linux Bash script or Windows batch/cmd file.
For a full list of available command, please run

```sh
poclmembench --help
```

The process exits with status `0` when all chunks passed their checks and
with a non zero status when `FAILED` was reported, so it can be used
directly in scripts and CI.

## Verification

Every chunk is checked after it has been benchmarked:

*	**CRC** - the whole chunk is read back and its CRC-32 is compared with the expected value
*	**artifacts** - every 32 bit word must contain the value written by the kernel, anything else is reported with its count and position
*	**errors** - every OpenCL call (program build, kernel enqueue, event wait, profiling, read back, queue finish) must return `CL_SUCCESS`, the failing call and error code are printed

A passing chunk ends with `OK`, a failing one with `FAILED`, and the run
finishes with an overall `RESULT` line:

```
Chunk:   0 (   0- 128)MB Speed: 90.39 GByte/s crc:0x12eb2da6 OK
Chunk:   1 ( 128- 256)MB Speed: 90.35 GByte/s crc:0x05ec52d1 artifacts:2(first@1234) crc_mismatch(expected:0x12eb2da6) FAILED
RESULT: FAILED (1/2 chunks with crc/error/artifact problems)
```

## Build

This project uses [CMake] and needs a C++11 compiler, Boost (*program_options*)
and the OpenCL headers/loader.

1. Create a build directory.

```sh
mkdir build; cd build
```

2. Configure the project with CMake.

```sh
cmake ..
```

3. Build the project using CMake. This is a portable variant of `make`.

```sh
cmake --build .
```
   

## example
```sh
poclmembench -p 0 -d 0
```
![Linux Screeenshot](img/ScreenshotLin.png?raw=true "Linux Screenshot")
![MacOS Screeenshot](img/ScreenshotMac.png?raw=true "MacOS Screenshot")
![Windows Screeenshot](img/ScreenshotWin.png?raw=true "Windows Screenshot")


