/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* defined to 1 if libfabric was configured with --enable-debug, 0 otherwise
   */
#define ENABLE_DEBUG 0

/* Define to 1 if the linker supports alias attribute. */
#define HAVE_ALIAS_ATTRIBUTE 1

/* Set to 1 to use c11 atomic functions */
#define HAVE_ATOMICS 1

/* Define to 1 if clock_gettime is available. */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if host_clock_get_service is available. */
#define HAVE_HOST_GET_CLOCK_SERVICE 0

/* Define to 1 if you have the <infiniband/verbs.h> header file. */
/* #undef HAVE_INFINIBAND_VERBS_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `dl' library (-ldl). */
#define HAVE_LIBDL 1

/* Whether we have libl or libnl3 */
#define HAVE_LIBNL3 0

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* mxm provider is built */
#define HAVE_MXM 0

/* mxm provider is built as DSO */
#define HAVE_MXM_DL 0

/* Define to 1 if you have the <netlink/netlink.h> header file. */
#define HAVE_NETLINK_NETLINK_H 1

/* Define to 1 if you have the <netlink/version.h> header file. */
/* #undef HAVE_NETLINK_VERSION_H */

/* psm provider is built */
#define HAVE_PSM 0

/* psm2 provider is built */
#define HAVE_PSM2 0

/* psm2 provider is built as DSO */
#define HAVE_PSM2_DL 0

/* Define to 1 if you have the <psm2.h> header file. */
/* #undef HAVE_PSM2_H */

/* psm provider is built as DSO */
#define HAVE_PSM_DL 0

/* Define to 1 if you have the <psm.h> header file. */
/* #undef HAVE_PSM_H */

/* Define to 1 if you have the <rdma/rsocket.h> header file. */
/* #undef HAVE_RDMA_RSOCKET_H */

/* sockets provider is built */
#define HAVE_SOCKETS 1

/* sockets provider is built as DSO */
#define HAVE_SOCKETS_DL 0

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if compiler/linker support symbol versioning. */
#define HAVE_SYMVER_SUPPORT 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* usnic provider is built */
#define HAVE_USNIC 1

/* usnic provider is built as DSO */
#define HAVE_USNIC_DL 0

/* verbs provider is built */
#define HAVE_VERBS 0

/* verbs provider is built as DSO */
#define HAVE_VERBS_DL 0

/* Define to 1 to enable valgrind annotations */
/* #undef INCLUDE_VALGRIND */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "libfabric"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "ofiwg@lists.openfabrics.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libfabric"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libfabric 1.2.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libfabric"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.2.0"

/* Define to 1 if pthread_spin_init is available. */
#define PT_LOCK_SPIN 1

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.2.0"
