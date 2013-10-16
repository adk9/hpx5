# Copyright (c) 2013, Trustees of Indiana University 
# All rights reserved.

# This software may be modified and distributed under the terms of
# the BSD license.  See the COPYING file for details.

# This software was created at the Indiana University Center for
# Research in Extreme Scale Technologies (CREST).

AC_PREREQ([2.63])
AC_INIT([hpx], [5.0.0a1], [hpx@indiana.edu])
CLEANFILES="*~ .\#* .gdb_history"
AC_SUBST(CLEANFILES)

AC_CONFIG_SRCDIR([include/hpx.h])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_LANG([C])

# Checks for programs.
AC_PROG_CC([icc gcc])
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AM_PROG_AS

#AS_IF([test "x$CC" = "icc"], [CFLAGS += "-no-gcc"])

AC_CHECK_LIB([m], [pow])
AC_CHECK_LIB([pthread], [pthread_self])

# Checks for header files.
AC_CHECK_HEADERS([limits.h mach/mach.h stdbool.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_C_CONST
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([clock_gettime memset pow])

dnl Allow users to build libhpx with pedantic warnings.
AC_ARG_ENABLE([pedantic],
    [AS_HELP_STRING([--enable-pedantic],
        [Enable -pedantic @<:@default=yes@:>@])],
    [], [enable_pedantic=yes])
AS_IF([test "x$enable_pedantic" != xno], AC_SUBST([HPX_PEDANTIC], [-pedantic]))

dnl Allow users to build libhpx with -Wall warnings.
AC_ARG_ENABLE([wall],
    [AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@])],
    [], [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno], AC_SUBST([HPX_W_ALL], [-Wall]))

dnl Allow users to build libhpx with warnings treated as errors. 
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=yes@:>@])],
    [], [enable_werror=yes])
AS_IF([test "x$enable_werror" != xno], AC_SUBST([HPX_W_ERROR], [-Werror]))

dnl Allow users to build libhpx with link-time-optimization.
AC_ARG_ENABLE([lto],
    [AS_HELP_STRING([--enable-lto],
        [Enable link-time optimization @<:@default=no@:>@])],
    [], [enable_lto=no])
AS_IF([test "x$enable_lto" != xno], AC_SUBST([HPX_F_LTO], [-flto]))

dnl Allow users to disable the testsuite.
AC_ARG_ENABLE([testsuite],
    [AS_HELP_STRING([--enable-testsuite],
        [Enable test suite (requires check) @<:@default=yes@:>@])],
    [], [enable_testsuite=yes])
    
dnl Use from libcrypto to hash action string keys.
AC_ARG_WITH([libcrypto],
    [AS_HELP_STRING([--with-libcrypto],
         [Build with openssl's libcrypto for string hashing @<:@default=yes@:>@])],
    [], [with_libcrypto=yes])
AS_IF([test "x$with_libcrypto" != xno],
    [PKG_CHECK_MODULES([LIBCRYPTO], [libcrypto])
     AC_DEFINE([HAVE_LIBCRYPTO], [], [Enable libcrypto for string hashing])
     AC_SUBST(LIBCRYPTO_PKG, [libcrypto])
     AC_SUBST(LIBCRYPTO_CFLAGS)
     AC_SUBST(LIBCRYPTO_LIBS)])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "x$with_libcrypto" != xno])

# Check for PAPI measurement support.
AC_ARG_WITH([papi],
    [AS_HELP_STRING([--with-papi],
        [Build with PAPI measurement support @<:@default=no@:>@])],
    [], [with_papi=no])
AS_IF([test "x$with_papi" != xno],
    [PKG_CHECK_MODULES([PAPI], [papi])
     AC_DEFINE([HAVE_PAPI], [], [Enable PAPI measurement support])
     AC_SUBST(PAPI_PKG, [papi])
     AC_SUBST(PAPI_CFLAGS)
     AC_SUBST(PAPI_LIBS)])

# Check for HWLOC.
AC_ARG_WITH([hwloc],
     [AS_HELP_STRING([--with-hwloc],
         [Build with HWLOC support @<:@default=no@:>@])],
     [], [with_hwloc=no])
AS_IF([test "x$with_hwloc" != xno],
    [PKG_CHECK_MODULES([HWLOC], [hwloc])
     AC_DEFINE([HAVE_HWLOC], [], [Enable HWLOC support])
     AC_SUBST(HWLOC_PKG, [hwloc])
     AC_SUBST(HWLOC_CFLAGS)
     AC_SUBST(HWLOC_LIBS)])

# Check for TCMALLOC.
AC_ARG_WITH([tcmalloc],
    [AS_HELP_STRING([--with-tcmalloc],
        [Build with TCMALLOC support @<:@default=no@:>@])],
    [], [with_tcmalloc=no])
AS_IF([test "x$with_tcmalloc" != xno],
    [PKG_CHECK_MODULES([TCMALLOC], [libtcmalloc])
     AC_DEFINE([HAVE_TCMALLOC], [], [Enable TCMALLOC support])
     AC_SUBST(TCMALLOC_PKG, [libtcmalloc])
     AC_SUBST(TCMALLOC_CFLAGS)
     AC_SUBST(TCMALLOC_LIBS)])

# Check for MPI
AC_ARG_WITH([mpi],
    [AS_HELP_STRING([--with-mpi[[=PKG]]],
        [Build with MPI support @<:@default=yes@:>@])],
    [], [with_mpi=yes])
AS_IF([test "x$with_mpi" != xno],
    [AC_DEFINE([HAVE_MPI], [1], [Enable MPI support])
     AS_IF([test "x$with_mpi" != xyes],
         [PKG_CHECK_MODULES([MPI], $with_mpi)
          AC_SUBST(MPI_PKG, [$with_mpi])
          AC_SUBST(MPI_CFLAGS)
          AC_SUBST(MPI_LIBS)])])
              
# Check for PHOTON
AC_ARG_WITH([photon],
    [AS_HELP_STRING([--with-photon],
        [Build with Photon support @<:@default=no@:>@])],
    [], [with_photon=no])
AS_IF([test "x$with_photon" != xno],
    [AC_DEFINE([HAVE_PHOTON], [], [Enable Photon support])
     AC_SUBST([PHOTON_CFLAGS], [])
     AC_SUBST([PHOTON_LIBS], '-lphoton -libverbs -lrdmacm')])

# Check for check (and disable test-suite if not found).
AC_ARG_WITH([check],
    [AS_HELP_STRING([--with-check[[=PKG]]],
        [Build with check unit testing @<:@default=yes@:>@])],
    [], [with_check=yes])
    
AS_IF([test "x$enable_testsuite" != xno],
    [AS_IF([test "x$with_check" != xno],
        [AS_IF([test "x$with_check" != xyes],
            [PKG_CHECK_MODULES([CHECK], [$with_check],
                [enable_tests=yes
                 AC_SUBST(CHECK_PKG, [$with_check])
                 AC_SUBST(CHECK_CFLAGS)
                 AC_SUBST(CHECK_LIBS)],
                [AC_MSG_WARN([Check not found. Install Check to enable the test-suite.])])],
            [PKG_CHECK_MODULES([CHECK], [ check >= 0.9.4 ],
                [enable_tests=yes
                 AC_SUBST(CHECK_PKG, [check])
                 AC_SUBST(CHECK_CFLAGS)
                 AC_SUBST(CHECK_LIBS)],
                [AC_MSG_WARN([Check not found. Install Check to enable the test-suite.])])])])])

AS_IF([test "x$enable_tests" = xyes],
    [AM_CONDITIONAL([ENABLE_TESTSUITE], true) test_string="yes"
     AC_CONFIG_FILES([tests/Makefile])],
    [AM_CONDITIONAL([ENABLE_TESTSUITE], false) test_string="no"])
     
# Check for doxygen (to build the documentation).
AC_ARG_WITH([doxygen],
    [AS_HELP_STRING([--with-doxygen[[=PKG]]],
        [Use doxygen to build documentation @<:@default=no@:>@])],
    [], [with_doxygen=no])
    
AS_IF([test "x$with_doxygen" != xno],
    [AS_IF([test "x$with_doxygen" != xyes],
        [PKG_CHECK_MODULES([DOXYGEN], [$with_doxygen],
             [enable_docs=yes],
             [AC_MSG_WARN([Doxygen not found, disabling source documentation build.])])],
        [AC_CHECK_PROGS([DOXYGEN], [doxygen])
         AS_IF([test -n "$DOXYGEN"],
             [enable_docs=yes],
             [AC_MSG_WARN([Doxygen not found, disabling source documentation build.])])])])

AS_IF([test "x$enable_docs" = xyes],
   [AM_CONDITIONAL([WITH_DOXYGEN], true)
    AM_CONDITIONAL([ENABLE_DOCS], true)
    doc_string="yes"
    dnl AC_DEFINE([ENABLE_DOCS], [], [Enable source documentation])
    AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])],
   [AM_CONDITIONAL([ENABLE_DOCS], false)
    AM_CONDITIONAL([WITH_DOXYGEN], false)
    doc_string="no"])

dnl Do some host-specific stuff
AS_CASE([$host_os],
    [linux*],   [AC_SUBST([PLATFORM_LIBS], [-lrt])
                 l1_dcashe_linesize=`getconf LEVEL1_DCACHE_LINESIZE`],
    [darwin*],  [l1_dcashe_linesize=`sysctl -n hw.cachelinesize`],
    [solaris*], [l1_dcashe_linesize=`prtpicl -v -c cpu | \
                                     grep l1-dcache-line-size | uniq | \
                                     awk '{print $2}'`]
                [l1_dcashe_linesize=128])

dnl Add static, configure-time values for some data we might want
AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], $l1_dcashe_linesize, [L1 D$ Line size])
AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], `getconf PAGESIZE`, [OS memory page size])

AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([PLATFORM_LINUX], [test "x$host_os" = xlinux-gnu])
AM_CONDITIONAL([HAVE_PAPI], [test "x$with_papi" != xno])
AM_CONDITIONAL([HAVE_HWLOC], [test "x$with_hwloc" != xno])
AM_CONDITIONAL([HAVE_TCMALLOC], [test "x$with_tcmalloc" != xno])
AM_CONDITIONAL([HAVE_MPI], [test "x$with_mpi" != xno])
AM_CONDITIONAL([HAVE_PHOTON], [test "x$with_photon" != xno])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$with_photon" != xno]) # TODO: make this robust

AC_CONFIG_FILES([
  Makefile
  libsync/Makefile
  src/Makefile
  include/Makefile
  examples/Makefile
  examples/hpx/Makefile	
  examples/thread/Makefile
  examples/network/Makefile
])
AC_OUTPUT

AS_IF([test "x$with_papi" != xno],
      [papi_string="yes"],
      [papi_string="no"])
AS_IF([test "x$with_hwloc" != xno],
      [hwloc_string="yes"],
      [hwloc_string="no"])
AS_IF([test "x$with_tcmalloc" != xno],
      [tcmalloc_string="yes"],
      [tcmalloc_string="no"])
AS_IF([test "x$with_mpi" != xno],
      [networks_string="$networks_string MPI"],
      [])
AS_IF([test "x$with_photon" != xno],
      [networks_string="$networks_string Photon"],
      [])
AS_IF([test "x$with_libcrypto" != xno],
      [hashstr_string="external (libcrypto)"],
      [hashstr_string="builtin"])

echo ""
echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "               Host CPU : $host_cpu"
echo "                Host OS : $host_os"
echo "         String Hashing : $hashstr_string"
echo "               Networks : $networks_string"
echo "              Profiling : $papi_string (PAPI)"
echo "     Topology Awareness : $hwloc_string (HWLOC)"
echo "  Thread-Caching Malloc : $tcmalloc_string (TCMALLOC)"
echo "          Documentation : $doc_string"
echo "             Test Suite : $test_string"
echo "================================================"

