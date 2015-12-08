/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.ac by autoheader.  */

/* Enable debugging stuff */
#define ENABLE_DEBUG 1

/* Enable instrumentationging stuff */
/* #undef ENABLE_INSTRUMENTATION */

/* Enable loggging stuff */
#define ENABLE_LOGGING 1

/* Enable profiling support */
/* #undef ENABLE_PROFILING */

/* We have agas */
/* #undef HAVE_AGAS */

/* We have a GAS allocator available */
#define HAVE_ALLOCATOR 1

/* We have apex support */
/* #undef HAVE_APEX */

/* We have global memory */
#define HAVE_AS_GLOBAL 1

/* We have registered memory */
#define HAVE_AS_REGISTERED 1

/* Define to 1 if the system has the type `CACHE_DESCRIPTOR'. */
/* #undef HAVE_CACHE_DESCRIPTOR */

/* Define to 1 if the system has the type `CACHE_RELATIONSHIP'. */
/* #undef HAVE_CACHE_RELATIONSHIP */

/* Define if the assembler supports CFI directives. */
#define HAVE_CFI_DIRECTIVES 1

/* Define to 1 if you have the `clz' function. */
/* #undef HAVE_CLZ */

/* Define to 1 if you have the `clzl' function. */
/* #undef HAVE_CLZL */

/* Define to 1 if you have the <CL/cl_ext.h> header file. */
/* #undef HAVE_CL_CL_EXT_H */

/* Have 16-byte compare and swap */
#define HAVE_CMPXCHG16B 1

/* Define to 1 if you have the `cpuset_setaffinity' function. */
/* #undef HAVE_CPUSET_SETAFFINITY */

/* Define to 1 if you have the `cpuset_setid' function. */
/* #undef HAVE_CPUSET_SETID */

/* Define to 1 if you have the <ctype.h> header file. */
/* #undef HAVE_CTYPE_H */

/* Define to 1 if we have -lcuda */
/* #undef HAVE_CUDA */

/* Define to 1 if you have the <cuda.h> header file. */
/* #undef HAVE_CUDA_H */

/* Define to 1 if you have the <cuda_runtime_api.h> header file. */
/* #undef HAVE_CUDA_RUNTIME_API_H */

/* define if the compiler supports basic C++11 syntax */
/* #undef HAVE_CXX11 */

/* Define to 1 if you have the declaration of `CL_DEVICE_TOPOLOGY_AMD', and to
   0 if you don't. */
/* #undef HAVE_DECL_CL_DEVICE_TOPOLOGY_AMD */

/* Define to 1 if you have the declaration of `CTL_HW', and to 0 if you don't.
   */
/* #undef HAVE_DECL_CTL_HW */

/* Define to 1 if you have the declaration of `fabsf', and to 0 if you don't.
   */
/* #undef HAVE_DECL_FABSF */

/* Define to 1 if you have the declaration of `getexecname', and to 0 if you
   don't. */
/* #undef HAVE_DECL_GETEXECNAME */

/* Define to 1 if you have the declaration of `GetModuleFileName', and to 0 if
   you don't. */
/* #undef HAVE_DECL_GETMODULEFILENAME */

/* Define to 1 if you have the declaration of `getprogname', and to 0 if you
   don't. */
/* #undef HAVE_DECL_GETPROGNAME */

/* Define to 1 if you have the declaration of `HW_NCPU', and to 0 if you
   don't. */
/* #undef HAVE_DECL_HW_NCPU */

/* Define to 1 if you have the declaration of
   `nvmlDeviceGetMaxPcieLinkGeneration', and to 0 if you don't. */
/* #undef HAVE_DECL_NVMLDEVICEGETMAXPCIELINKGENERATION */

/* Define to 1 if you have the declaration of `pthread_getaffinity_np', and to
   0 if you don't. */
/* #undef HAVE_DECL_PTHREAD_GETAFFINITY_NP */

/* Define to 1 if you have the declaration of `pthread_setaffinity_np', and to
   0 if you don't. */
/* #undef HAVE_DECL_PTHREAD_SETAFFINITY_NP */

/* Define to 1 if you have the declaration of `strtoull', and to 0 if you
   don't. */
/* #undef HAVE_DECL_STRTOULL */

/* Define to 1 if you have the declaration of `_SC_LARGE_PAGESIZE', and to 0
   if you don't. */
/* #undef HAVE_DECL__SC_LARGE_PAGESIZE */

/* Define to 1 if you have the declaration of `_SC_NPROCESSORS_CONF', and to 0
   if you don't. */
/* #undef HAVE_DECL__SC_NPROCESSORS_CONF */

/* Define to 1 if you have the declaration of `_SC_NPROCESSORS_ONLN', and to 0
   if you don't. */
/* #undef HAVE_DECL__SC_NPROCESSORS_ONLN */

/* Define to 1 if you have the declaration of `_SC_NPROC_CONF', and to 0 if
   you don't. */
/* #undef HAVE_DECL__SC_NPROC_CONF */

/* Define to 1 if you have the declaration of `_SC_NPROC_ONLN', and to 0 if
   you don't. */
/* #undef HAVE_DECL__SC_NPROC_ONLN */

/* Define to 1 if you have the declaration of `_SC_PAGESIZE', and to 0 if you
   don't. */
/* #undef HAVE_DECL__SC_PAGESIZE */

/* Define to 1 if you have the declaration of `_SC_PAGE_SIZE', and to 0 if you
   don't. */
/* #undef HAVE_DECL__SC_PAGE_SIZE */

/* Define to 1 if you have the <dirent.h> header file. */
/* #undef HAVE_DIRENT_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* We have dlmalloc */
/* #undef HAVE_DLMALLOC */

/* Define to 1 if you have the `ffs' function. */
/* #undef HAVE_FFS */

/* Define to 1 if you have the `ffsl' function. */
/* #undef HAVE_FFSL */

/* Define to 1 if you have the `fls' function. */
/* #undef HAVE_FLS */

/* Define to 1 if you have the `flsl' function. */
/* #undef HAVE_FLSL */

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define to 1 if the system has the type `GROUP_AFFINITY'. */
/* #undef HAVE_GROUP_AFFINITY */

/* Define to 1 if the system has the type `GROUP_RELATIONSHIP'. */
/* #undef HAVE_GROUP_RELATIONSHIP */

/* Define to 1 if you have the `host_info' function. */
/* #undef HAVE_HOST_INFO */

/* Have support for hugetlbfs */
/* #undef HAVE_HUGETLBFS */

/* We have hwloc support */
#define HAVE_HWLOC 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* We have the jemalloc allocator */
#define HAVE_JEMALLOC 1

/* Define to 1 if the system has the type `KAFFINITY'. */
/* #undef HAVE_KAFFINITY */

/* Define to 1 if you have the <kstat.h> header file. */
/* #undef HAVE_KSTAT_H */

/* We have the libcuckoo hash table */
/* #undef HAVE_LIBCUCKOO */

/* We have libffi */
#define HAVE_LIBFFI 1

/* Define to 1 if we have -lgdi32 */
/* #undef HAVE_LIBGDI32 */

/* Define to 1 if we have -lkstat */
/* #undef HAVE_LIBKSTAT */

/* Define to 1 if we have -llgrp */
/* #undef HAVE_LIBLGRP */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the <libudev.h> header file. */
/* #undef HAVE_LIBUDEV_H */

/* Define to 1 if the system has the type `LOGICAL_PROCESSOR_RELATIONSHIP'. */
/* #undef HAVE_LOGICAL_PROCESSOR_RELATIONSHIP */

/* Define to 1 if you have the <mach/mach_host.h> header file. */
/* #undef HAVE_MACH_MACH_HOST_H */

/* Define to 1 if you have the <mach/mach_init.h> header file. */
/* #undef HAVE_MACH_MACH_INIT_H */

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the `memalign' function. */
/* #undef HAVE_MEMALIGN */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* MPI support available */
#define HAVE_MPI 1

/* We have a high speed network available */
#define HAVE_NETWORK 1

/* Define to 1 if you have the <numaif.h> header file. */
/* #undef HAVE_NUMAIF_H */

/* Define to 1 if the system has the type `NUMA_NODE_RELATIONSHIP'. */
/* #undef HAVE_NUMA_NODE_RELATIONSHIP */

/* Define to 1 if you have the <NVCtrl/NVCtrl.h> header file. */
/* #undef HAVE_NVCTRL_NVCTRL_H */

/* Define to 1 if you have the <nvml.h> header file. */
/* #undef HAVE_NVML_H */

/* Define to 1 if you have the `openat' function. */
/* #undef HAVE_OPENAT */

/* Have support for opencl */
/* #undef HAVE_OPENCL */

/* Have support for papi */
/* #undef HAVE_PAPI */

/* We have percolation */
/* #undef HAVE_PERCOLATION */

/* Photon support available */
#define HAVE_PHOTON 1

/* Define to 1 if you have the <picl.h> header file. */
/* #undef HAVE_PICL_H */

/* Enable PMI */
/* #undef HAVE_PMI */

/* Enable Cray PMI */
/* #undef HAVE_PMI_CRAY_EXT */

/* Define to 1 if you have the `posix_memalign' function. */
/* #undef HAVE_POSIX_MEMALIGN */

/* Define to 1 if the system has the type `PROCESSOR_CACHE_TYPE'. */
/* #undef HAVE_PROCESSOR_CACHE_TYPE */

/* Define to 1 if the system has the type `PROCESSOR_GROUP_INFO'. */
/* #undef HAVE_PROCESSOR_GROUP_INFO */

/* Define to 1 if the system has the type `PROCESSOR_RELATIONSHIP'. */
/* #undef HAVE_PROCESSOR_RELATIONSHIP */

/* Define to '1' if program_invocation_name is present and usable */
/* #undef HAVE_PROGRAM_INVOCATION_NAME */

/* Define to 1 if the system has the type `PSAPI_WORKING_SET_EX_BLOCK'. */
/* #undef HAVE_PSAPI_WORKING_SET_EX_BLOCK */

/* Define to 1 if the system has the type `PSAPI_WORKING_SET_EX_INFORMATION'.
   */
/* #undef HAVE_PSAPI_WORKING_SET_EX_INFORMATION */

/* Define to 1 if you have the <pthread_np.h> header file. */
/* #undef HAVE_PTHREAD_NP_H */

/* Have PTHREAD_PRIO_INHERIT. */
#define HAVE_PTHREAD_PRIO_INHERIT 1

/* Define to 1 if the system has the type `pthread_t'. */
/* #undef HAVE_PTHREAD_T */

/* Define to 1 if the system has the type `RelationProcessorPackage'. */
/* #undef HAVE_RELATIONPROCESSORPACKAGE */

/* Define to 1 if you have the `setlocale' function. */
/* #undef HAVE_SETLOCALE */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strftime' function. */
/* #undef HAVE_STRFTIME */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
/* #undef HAVE_STRNCASECMP */

/* Define to '1' if sysctl is present and usable */
/* #undef HAVE_SYSCTL */

/* Define to '1' if sysctlbyname is present and usable */
/* #undef HAVE_SYSCTLBYNAME */

/* Define to 1 if the system has the type
   `SYSTEM_LOGICAL_PROCESSOR_INFORMATION'. */
/* #undef HAVE_SYSTEM_LOGICAL_PROCESSOR_INFORMATION */

/* Define to 1 if the system has the type
   `SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX'. */
/* #undef HAVE_SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX */

/* Define to 1 if you have the <sys/cpuset.h> header file. */
/* #undef HAVE_SYS_CPUSET_H */

/* Define to 1 if you have the <sys/lgrp_user.h> header file. */
/* #undef HAVE_SYS_LGRP_USER_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
/* #undef HAVE_SYS_SYSCTL_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/utsname.h> header file. */
/* #undef HAVE_SYS_UTSNAME_H */

/* We have the tbbmalloc allocator */
/* #undef HAVE_TBBMALLOC */

/* Define to 1 if you have the `uname' function. */
/* #undef HAVE_UNAME */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <X11/keysym.h> header file. */
/* #undef HAVE_X11_KEYSYM_H */

/* Define to 1 if you have the <X11/Xlib.h> header file. */
/* #undef HAVE_X11_XLIB_H */

/* Define to 1 if you have the <X11/Xutil.h> header file. */
/* #undef HAVE_X11_XUTIL_H */

/* Define to 1 if the system has the `__builtin_clz' built-in function */
#define HAVE___BUILTIN_CLZ 1

/* Define to 1 if the system has the `__builtin_clzl' built-in function */
#define HAVE___BUILTIN_CLZL 1

/* Define to 1 if the system has the `__builtin_ctzl' built-in function */
#define HAVE___BUILTIN_CTZL 1

/* Define to 1 if the system has the `__builtin_expect' built-in function */
#define HAVE___BUILTIN_EXPECT 1

/* Define to 1 if the system has the `__builtin_popcountl' built-in function
   */
#define HAVE___BUILTIN_POPCOUNTL 1

/* Define to 1 if the system has the `__builtin_unreachable' built-in function
   */
#define HAVE___BUILTIN_UNREACHABLE 1

/* Define to '1' if __progname is present and usable */
/* #undef HAVE___PROGNAME */

/* Cacheline size */
#define HPX_CACHELINE_SIZE 64

/* OS Memory Page Size */
#define HPX_PAGE_SIZE 4096

/* The HPX interface version */
#define HPX_VERSION "2.0.0"

/* Define to 1 on AIX */
/* #undef HWLOC_AIX_SYS */

/* Define to 1 on BlueGene/Q */
/* #undef HWLOC_BGQ_SYS */

/* Whether C compiler supports symbol visibility or not */
/* #undef HWLOC_C_HAVE_VISIBILITY */

/* Define to 1 on Darwin */
/* #undef HWLOC_DARWIN_SYS */

/* Whether we are in debugging mode or not */
/* #undef HWLOC_DEBUG */

/* Define to 1 on *FREEBSD */
/* #undef HWLOC_FREEBSD_SYS */

/* Whether your compiler has __attribute__ or not */
/* #undef HWLOC_HAVE_ATTRIBUTE */

/* Whether your compiler has __attribute__ aligned or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_ALIGNED */

/* Whether your compiler has __attribute__ always_inline or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_ALWAYS_INLINE */

/* Whether your compiler has __attribute__ cold or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_COLD */

/* Whether your compiler has __attribute__ const or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_CONST */

/* Whether your compiler has __attribute__ deprecated or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_DEPRECATED */

/* Whether your compiler has __attribute__ format or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_FORMAT */

/* Whether your compiler has __attribute__ hot or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_HOT */

/* Whether your compiler has __attribute__ malloc or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_MALLOC */

/* Whether your compiler has __attribute__ may_alias or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_MAY_ALIAS */

/* Whether your compiler has __attribute__ nonnull or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_NONNULL */

/* Whether your compiler has __attribute__ noreturn or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_NORETURN */

/* Whether your compiler has __attribute__ no_instrument_function or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_NO_INSTRUMENT_FUNCTION */

/* Whether your compiler has __attribute__ packed or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_PACKED */

/* Whether your compiler has __attribute__ pure or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_PURE */

/* Whether your compiler has __attribute__ sentinel or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_SENTINEL */

/* Whether your compiler has __attribute__ unused or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_UNUSED */

/* Whether your compiler has __attribute__ warn unused result or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_WARN_UNUSED_RESULT */

/* Whether your compiler has __attribute__ weak alias or not */
/* #undef HWLOC_HAVE_ATTRIBUTE_WEAK_ALIAS */

/* Define to 1 if your `ffs' function is known to be broken. */
/* #undef HWLOC_HAVE_BROKEN_FFS */

/* Define to 1 if you have the `clz' function. */
/* #undef HWLOC_HAVE_CLZ */

/* Define to 1 if you have the `clzl' function. */
/* #undef HWLOC_HAVE_CLZL */

/* Define to 1 if the CPU_SET macro works */
/* #undef HWLOC_HAVE_CPU_SET */

/* Define to 1 if the CPU_SET_S macro works */
/* #undef HWLOC_HAVE_CPU_SET_S */

/* Define to 1 if you have the `cudart' SDK. */
/* #undef HWLOC_HAVE_CUDART */

/* Define to 1 if function `clz' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_CLZ */

/* Define to 1 if function `clzl' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_CLZL */

/* Define to 1 if function `ffs' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_FFS */

/* Define to 1 if function `ffsl' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_FFSL */

/* Define to 1 if function `fls' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_FLS */

/* Define to 1 if function `flsl' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_FLSL */

/* Define to 1 if function `strncasecmp' is declared by system headers */
/* #undef HWLOC_HAVE_DECL_STRNCASECMP */

/* Define to 1 if you have the `ffs' function. */
/* #undef HWLOC_HAVE_FFS */

/* Define to 1 if you have the `ffsl' function. */
/* #undef HWLOC_HAVE_FFSL */

/* Define to 1 if you have the `fls' function. */
/* #undef HWLOC_HAVE_FLS */

/* Define to 1 if you have the `flsl' function. */
/* #undef HWLOC_HAVE_FLSL */

/* Define to 1 if you have the GL module components. */
/* #undef HWLOC_HAVE_GL */

/* Define to 1 if you have the `libxml2' library. */
/* #undef HWLOC_HAVE_LIBXML2 */

/* Define to 1 if building the Linux PCI component */
/* #undef HWLOC_HAVE_LINUXPCI */

/* Define to 1 if mbind is available. */
/* #undef HWLOC_HAVE_MBIND */

/* Define to 1 if migrate_pages is available. */
/* #undef HWLOC_HAVE_MIGRATE_PAGES */

/* Define to 1 if you have the `NVML' library. */
/* #undef HWLOC_HAVE_NVML */

/* Define to 1 if glibc provides the old prototype (without length) of
   sched_setaffinity() */
/* #undef HWLOC_HAVE_OLD_SCHED_SETAFFINITY */

/* Define to 1 if you have the `OpenCL' library. */
/* #undef HWLOC_HAVE_OPENCL */

/* Define to 1 if the hwloc library should support dynamically-loaded plugins
   */
/* #undef HWLOC_HAVE_PLUGINS */

/* `Define to 1 if you have pthread_getthrds_np' */
/* #undef HWLOC_HAVE_PTHREAD_GETTHRDS_NP */

/* Define to 1 if pthread mutexes are available */
/* #undef HWLOC_HAVE_PTHREAD_MUTEX */

/* Define to 1 if glibc provides a prototype of sched_setaffinity() */
/* #undef HWLOC_HAVE_SCHED_SETAFFINITY */

/* Define to 1 if set_mempolicy is available. */
/* #undef HWLOC_HAVE_SET_MEMPOLICY */

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HWLOC_HAVE_STDINT_H */

/* Define to 1 if you have the `windows.h' header. */
/* #undef HWLOC_HAVE_WINDOWS_H */

/* Define to 1 if X11 headers including Xutil.h and keysym.h are available. */
/* #undef HWLOC_HAVE_X11_KEYSYM */

/* Define to 1 if you have x86 cpuid */
/* #undef HWLOC_HAVE_X86_CPUID */

/* Define to 1 if the _syscall3 macro works */
/* #undef HWLOC_HAVE__SYSCALL3 */

/* Define to 1 on HP-UX */
/* #undef HWLOC_HPUX_SYS */

/* Define to 1 on Irix */
/* #undef HWLOC_IRIX_SYS */

/* Define to 1 on Linux */
/* #undef HWLOC_LINUX_SYS */

/* Define to 1 on *NETBSD */
/* #undef HWLOC_NETBSD_SYS */

/* Define to 1 on OSF */
/* #undef HWLOC_OSF_SYS */

/* The size of `unsigned int', as computed by sizeof */
/* #undef HWLOC_SIZEOF_UNSIGNED_INT */

/* The size of `unsigned long', as computed by sizeof */
/* #undef HWLOC_SIZEOF_UNSIGNED_LONG */

/* Define to 1 on Solaris */
/* #undef HWLOC_SOLARIS_SYS */

/* The hwloc symbol prefix */
/* #undef HWLOC_SYM_PREFIX */

/* The hwloc symbol prefix in all caps */
/* #undef HWLOC_SYM_PREFIX_CAPS */

/* Whether we need to re-define all the hwloc public symbols or not */
/* #undef HWLOC_SYM_TRANSFORM */

/* Define to 1 on unsupported systems */
/* #undef HWLOC_UNSUPPORTED_SYS */

/* The library version, always available, even in embedded mode, contrary to
   VERSION */
/* #undef HWLOC_VERSION */

/* Define to 1 on WINDOWS */
/* #undef HWLOC_WIN_SYS */

/* Define to 1 on x86_32 */
/* #undef HWLOC_X86_32_ARCH */

/* Define to 1 on x86_64 */
/* #undef HWLOC_X86_64_ARCH */

/* The libhpx library version */
#define LIBHPX_VERSION "2.0.0"

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "hpx-5"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "hpx@indiana.edu"

/* Define to the full name of this package. */
#define PACKAGE_NAME "hpx-5"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "hpx-5 2.0.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "hpx-5"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.0.0"

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* The size of `unsigned int', as computed by sizeof. */
/* #undef SIZEOF_UNSIGNED_INT */

/* The size of `unsigned long', as computed by sizeof. */
/* #undef SIZEOF_UNSIGNED_LONG */

/* The size of `void *', as computed by sizeof. */
/* #undef SIZEOF_VOID_P */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Enable extensions on HP-UX. */
#ifndef _HPUX_SOURCE
/* # undef _HPUX_SOURCE */
#endif


/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "2.0.0"

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Are we building for HP-UX? */
/* #undef _HPUX_SOURCE */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define the POSIX version */
#define _POSIX_C_SOURCE 200809L

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define this to the process ID type */
/* #undef hwloc_pid_t */

/* Define this to the thread ID type */
/* #undef hwloc_thread_t */
