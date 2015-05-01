# HPX–5

Welcome to the HPX–5 runtime system library! In order to get started with 
HPX~5, you first need to know some basic information and how to go about using 
it. This document discusses how to get up and running quickly with the HPX–5 
Runtime System. Everything from unpacking and compilation of the distribution to
 execution of some tools.

## Building and Running a network-capable HPX–5

There are several pre-requisites to successfully configure and run HPX–5.

### Requirements

* GCC 4.8.4 or newer (Tested with 4.8.4, 4.9.1, 4.9.2)
* GNU Make 3.81+    
* autoconf 2.6.9     (only required for developer builds)
* automake 1.15      (only required for developer builds)
* GNU M4 1.4.17      (only required for developer builds)
* libtool 2.4.6      (only required for developer builds)
* pkg-config         (only required for developer builds)
* MPI                (optional - Tested with 1.6.3, 1.6.5, 1.8.1, 1.8.4 
                                 MPICH 3.0.4, mvapich2/2.0b (Stampede))
* doxygen            (optional)
* Photon             (embedded with HPX–5)
* jemalloc           (embedded with HPX–5)
* hwloc              (embedded with HPX–5)
* libffi             (embedded with HPX–5)
* Uthash             (embedded with HPX–5)
* Valgrind           (optional)

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

Documentation of configuration otpions can be accessed by running 
‘./configure --help’ from the top-level directory.

The external software packages are supported through pkg-config files.  For
example, to enable MPI, one has to provide `--with-mpi=PKG`, where `PKG` is the
name of the pkg-file to use. Note that pkg-files must be available through
pkg-config (see `man pkg-config` for details). The "with" options usually have
defaults, and the `--with-mpi` option will load `ompi` pkg-file by default.
If pkg-config file is not found, a warning will be printed, and the build system
will depend on environment variables to configure the requested software. This 
is particularly useful on Cray systems where the Cray compiler wrapper
automatically supports many software packages through Cray environment. So,
on Cray one could run `./configure ... --with-mpi ... CC=cc`, and since the 
default `ompi` module is not found, MPI will be picked up from the `cc` compiler 
wrapper.

#### HPX–5 Network Transports

HPX–5 can build and run successfully without any network backend, but at 
present, MPI or Photon is required for networking by HPX–5.

HPX–5 can be built with one, both, or none of the network transports depending
 on application needs. Each transport is runtime configurable. MPI and PMI are
 currently used as job launchers and bootstrap mechanism for HPX–5.

Note that if you are building with Photon, the libraries for the given network 
interconnect you are targeting need to be present on the build system. The two
 supported interconnects are InfiniBand (libibverbs and librdmacm) and Cray's 
GEMINI and ARIES via uGNI (libugni). You may build with IBV and/or uGNI support
 on a workstation where the development packages are installed, but launching an
 HPX–5 application with Photon requires that the actual network devices be 
present so the library can initialize.

If you build with Photon and/or MPI on a system without networking, you may 
still use the SMP transport to run applications that are not distributed.

#### Configuring on InfiniBand Systems

##### Configuring with MPI

```
$ ./configure --prefix=/path/to/install --with-mpi=ompi
```

On stampede use
```
$ module load intel/14.0.1.106
$ LDFLAGS=-L/opt/ofed/lib64 CPPFLAGS=-I/opt/ofed/include ./configure --with-mpi
           CC=mpicc
```

##### Configuring with Photon

The Photon transport is included in HPX–5 within the contrib directory. To 
configure HPX–5 with Photon use option --enable-photon

```
$ ./configure --prefix=/path/to/install/ --with-mpi=ompi --enable-photon
```

##### Configuring on CRAY Systems

```
$ module unload PrgEnv-cray
$ module unload PrgEnv-intel/5.2.40
$ module load PrgEnv-gnu
$ setenv CRAYPE_LINK_TYPE dynamic
$ ./configure --prefix=/path/to/install/ CC=cc
```

The above example switches to GNU environment, but HPX should build with 
intel module as well.

###### Configuring with MPI on cray

If you have MPICH installed, you need to specify: ‘$ ./configure
 --prefix=/path/to/install --with-mpi=mpich CC=cc’ will instruct ‘configure’ to
 look for ‘mpich.pc’.

###### Configuring with Photon on cray

```
$ ./configure --prefix=/path/to/install/ CC=cc --enable-photon --with-pmi 
--with-mpi HPX_PHOTON_CARGS="--with-ugni"
```

##### Configuring with the test-suite enabled

* To build and run the unit and performance testsuite enabled:

```
$ ./configure  --enable-testsuite --with-tests-cmd="mpirun -np 2" 
$ make check
```

--with-tests-cmd is used to specify the application launch command on various 
systems. This has to be set appropriately for your system.

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

Detailed build instructions can be found at Getting started with HPX–5 Runtime 
Systems in https://hpx.crest.iu.edu/documentation and FAQs in 
(https://hpx.crest.iu.edu/faqs_and_tutorials).
