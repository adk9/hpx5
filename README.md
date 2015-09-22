# HPX–5

Welcome to the HPX–5 runtime system library! In order to get started with 
HPX~5, you first need to know some basic information and how to go about using 
it. This document discusses how to get up and running quickly with the HPX–5 
Runtime System. Everything from unpacking and compilation of the distribution to
 execution of some tools.

## Building and Running a network-capable HPX–5

There are several pre-requisites to successfully configure and run HPX–5.

### Requirements

* GCC 4.8.4 or newer (Tested with 4.8.4, 4.9.1, 4.9.2, 5.1.0, 5.2.0).
* clang Tested with 3.6 (3.5 and earlier are known to be broken).
* icc (Tested with 15.0.1).
* GNU Make 3.81+    
* autoconf 2.6.9     (Only required if building from the git version of HPX–5)
* automake 1.15      (Only required if building from the git version of HPX–5)
* GNU M4 1.4.17      (Only required if building from the git version of HPX–5)
* libtool 2.4.6      (Only required if building from the git version of HPX–5)
* pkg-config         
* MPI                (optional - Tested with 1.6.3, 1.6.5, 1.8.1, 1.8.4, 1.10.0 
                                 MPICH 3.0.4, mvapich2/2.0b (Stampede))
* doxygen            (optional; required to build the documentation)
* Photon             (embedded with HPX–5)
* jemalloc           (embedded with HPX–5, If available, tbbmalloc may be used 
                      instead of jemalloc)
* hwloc              (embedded with HPX–5)
* libcuckoo          (embedded with HPX–5)
* libffi             (embedded with HPX–5)
* libffi-mic         (embedded with HPX–5, needed for Xeon-Phi)
* Uthash             (embedded with HPX–5)
* Valgrind           (optional)
* APEX               (optional, required to build with APEX)
* PAPI               (optional, required to build with PAPI)

HPX–5 can build and run successfully without any network backend, but at
 present, MPI or Photon is required for networking by HPX–5. 

The latest autotools can be installed by running a script provided in the 
distribution:
 
```
$ ./scripts/setup_autotools.sh $PATH
``` 
The script takes an path to install autotools to. After 
installing autotools, be sure to update your PATH variable. Only after 
installing the latest autotools should the user run ./bootstrap.

### Bootstrapping

Note: ./bootstrap should not be used with release tarballs.
 
HPX–5 provides a bootstrap script in its build. In the HPX–5 directory run the 
bootstrap script using

```
$ ./bootstrap
```

Bootstrap is a bash script that generate the initialization required to create 
a configure script when using GNU autotools. This calls the autoreconf.

### Configuration

The HPX–5 configuration will pull a number of dependencies from your 
environment. See ./configure --help for a complete list of the variables that 
are used.

DOXYGEN:	The doxygen executable for documentation build.
PHOTON_CARGS:	Additional configuration arguments for the included photon 
                package.
TBBROOT:	The path to the root of Intel's TBB installation.
TESTS_CMD:	A command to launch each integration test during make check.

The HPX–5 configuration looks for most dependencies in your current path. 
These dependencies will be found and included automatically as necessary. 
For missing dependencies HPX–5 will fall back to looking for pkg-config 
packages, which will also be found without intervention if they are 
installed on your system using their default names and their pkg.pc files 
are in your $PKG_CONFIG_PATH. You can force HPX–5 to use a package over the 
system-installed version of a dependency, or use an installation with a 
non-standard name, by specifying --with-package=pkg during configuration. 
See ./configure --help for further details.

#### HPX–5 Network Transports

HPX–5 can build and run successfully without any network backend, but at 
present, MPI or Photon is required for networking by HPX–5.

Note that if you are building with Photon, the libraries for the given network 
interconnect you are targeting need to be present on the build system. The two 
supported interconnects are InfiniBand (libibverbs and librdmacm) and Cray’s 
GEMINI and ARIES via uGNI (libugni).

If you build with Photon and/or MPI on a system without networking, you may 
still use the SMP option to run applications that are not distributed.

##### Configuring with MPI

By default, HPX–5 will be configured in SMP mode---without a high-speed network. 
To configure with MPI, use the --enable-mpi option. When MPI is enabled, the 
configuration will search for the appropriate way to include and link to MPI.

HPX–5 will try and see if hpx.h and libmpi.so are available with no additional 
flags (e.g., for CC=mpicc or on Cray CC=cc).

HPX–5 will test for an hpx.h and -lmpi in the current C_INCLUDE_PATH and 
{LD_}LIBARARY_PATH respectively.

HPX–5 will look for an ompi pkg-config package.
If you want to specify which MPI should be used or the build system cannot 
find the .pc file you may need to use the --with-mpi=pkg option. The pkg 
parameter can be either the prefix for the .pc file (ompi, mpich, etc.) or a 
complete path to the .pc file.

##### Configuring with Photon

The Photon network library is included in HPX–5 within the contrib directory. 
To configure HPX–5 with Photon use the option --enable-photon. HPX–5 can also 
use a system-installed or pkg-config version of Photon. This can be controlled 
using the --with-photon=system or --with-photon=pkg, respectively. Note that 
HPX–5 does not provide its own distributed job launcher, so it is necessary 
to use either the --enable-pmi or --enable-mpi option in addition to 
--enable-photon in order to build support for mpirun or aprun bootstrapping.

To configure on Cray machines you will need to include the  
PHOTON_CARGS=--with-ugni flag during configuration so that Photon builds with 
ugni support. In addition, the --with-hugetlbfs option causes the HPX–5 heap 
to be mapped with huge pages, which is necessary for larger heaps on some 
Cray Gemini machines. The hugepages modules provide the environment necessary 
for compilation. Huge page support should work where available, but we have 
seen no performance benefits to using it at this point.

##### Configuring with the test-suite enabled

* To build and run the unit and performance testsuite enabled add the 
--enable-testsuite option. It is usually necessary to set TESTS_CMD=<command> 
as well. For example,

```
$ ./configure TESTS_CMD="mpirun -np 2 --map-by node:PE=16" --enable-mpi
$ make check
```
### To complete the build and install use:

```
make
make install
```

### Running

The Photon default is to use the first detected IB device and active port. This
 behavior can be overridden with the following environment variable:

```
$ export HPX_PHOTON_IBDEV="mlx4_0"
```

This string also acts as a device filter. For example,

Device names can be retrieved with ibv_devinfo on systems with IB Verbs support. 
If HPX_PHOTON_IBDEV is set to be blank, Photon will try to automatically select
 the right device.

(These parameters can also be set at run time using the command line option 
--hpx-photon-ibdev=<...>.)

The Photon default backend is also set to verbs. On BigRed2, or any other Cray
 system with a uGNI-supported interconnect, set the following environment 
variable:

```
$ export HPX_PHOTON_BACKEND=ugni
```

OR, use --hpx-photon-backend=ugni as a command line flag.

In addition, for huge pages builds, the size of huge pages can be controlled:

```
$ export HUGETLB_DEFAULT_PAGE_SIZE=64M
```

The list of supported sizes can be obtained as follows:

```
$ module avail craype-hugepages
```

HPX–5 provides runtime options that can be specified on the command line or in
 the environment. The list of options can be obtained from any HPX–5 program by 
adding --hpx-help option.

HPX–5 programs can be run using any of the MPI or PMI launchers such as mpirun 
or mpiexec.
E.g. to run the pingpong example,

```
$ mpirun -np 2 examples/pingpong 10 -m -v
```

Detailed build instructions can be found in the User's Guide to HPX–5 found at https://hpx.crest.iu.edu/documentation and FAQs in 
(https://hpx.crest.iu.edu/faqs_and_tutorials).
