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

# NB: Don't check for anything that we're not actually prepared to deal with
#     in our source code. Add these commented-out checks later if we come up
#     with platform-independent code that actually requires the result of
#     these checks.

# ---------------------------------------------------------------------------
# --enable options
# ---------------------------------------------------------------------------
AC_ARG_ENABLE([pedantic],
  [AS_HELP_STRING([--enable-pedantic], [Enable -pedantic @<:@default=yes@:>@])],
  [], [enable_pedantic=yes])
AS_IF([test "x$enable_pedantic" != xno],
  [AC_SUBST([HPX_PEDANTIC], [-pedantic])])

AC_ARG_ENABLE([wall],
  [AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@])],
  [], [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno],
  [AC_SUBST([HPX_W_ALL], [-Wall])])

AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=yes@:>@])],
  [], [enable_werror=yes])
AS_IF([test "x$enable_werror" != xno],
  [AC_SUBST([HPX_W_ERROR], [-Werror])])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug code @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debugging stuff])])
    
AC_ARG_ENABLE([testsuite],
  [AS_HELP_STRING([--enable-testsuite], [Enable test suite (requires check) @<:@default=yes@:>@])],
  [], [enable_testsuite=yes])

# ---------------------------------------------------------------------------
# --with-options, see config/hpx_with_pkg.m4 for details
# ---------------------------------------------------------------------------
HPX_WITH_PKG([papi],[papi],[build with profiling],[no],[PAPI])
HPX_WITH_PKG([hwloc],[hwloc],[build with topology awareness],[no],[HWLOC])
HPX_WITH_PKG([tcmalloc],[tcmalloc],[build with explicit tcmalloc],[no],[TCMALLOC])
HPX_WITH_PKG([crypto],[libcrypto],[use libcrypto for string hashing],[no],[CRYPTO])
HPX_WITH_PKG([glib],[glib-2.0],[build with glib-2.0],[no],[GLIB])

HPX_WITH_PKG([mpi],[ompi],[build with MPI bootstrap],[yes],[MPI])
HPX_WITH_PKG([photon],[photon],[build with Photon networking],[no],[PHOTON])
AS_IF([test "x$with_mpi" != xno -o "x$with_photon" != xno],
  [AC_DEFINE([HAVE_NETWORK], [1], [Enable networking support])])
    
HPX_WITH_PKG([check],[check],[build with check unit tests],[no],[CHECK])
AS_IF([test "x$enable_testsuite" != xno -a "x$with_check" != xno],
  [enable_tests=yes],
  [enable_tests=no])
    
AC_ARG_WITH([doxygen],
  [AS_HELP_STRING([--with-doxygen], [build documentation @<:@default=no@:>@])],
  [], [with_doxygen=yes])
AS_IF([test "x$with_doxygen" != xno],
  [AC_CHECK_PROGS([DOXYGEN], [doxygen])])
AS_IF([test "x$DOXYGEN" != x],
  [enable_docs=yes],
  [AC_MSG_WARN([Doxygen not found, disabling source documentation build.])
   enable_docs=no])

# ---------------------------------------------------------------------------
# Perform host-specific work here.
# ---------------------------------------------------------------------------
AS_CASE([$host_os],
    [linux*],   [AC_SUBST([PLATFORM_LIBS], [-lrt])
                 AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
                 l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
                 pagesize=`getconf PAGESIZE`],
    [darwin*],  [l1d_linesize=`sysctl -n hw.cachelinesize`
                 pagesize=`getconf PAGESIZE`],
    [solaris*], [l1d_linesize=`prtpicl -v -c cpu | \
                                     grep l1-dcache-line-size | uniq | \
                                     awk '{print $2}'`]
    [AC_MSG_WARN([Unexpected Host OS $host_os, using defaults])
     l1d_linesize=128
     pagesize=4096])

AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], [$pagesize], [OS Memory Page Size])
    
# -----------------------------------------------------------------------------
# Set automake conditionals for use in Makefile.am settings.
# -----------------------------------------------------------------------------
AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$with_photon" != xno])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" != xno])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])

# -----------------------------------------------------------------------------
# Use autoconf to configure all of the Makefiles that need to be processed.
# -----------------------------------------------------------------------------
AC_CONFIG_FILES([
  Makefile
  libsync/Makefile
  src/Makefile
  src/libhpx.pc
  include/Makefile
  examples/Makefile
  examples/hpx/Makefile	
  examples/thread/Makefile
  examples/network/Makefile
])

AS_IF([test "x$enable_docs" != xno],
  [AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])])
AS_IF([test "x$enable_tests" != xno],
  [AC_CONFIG_FILES([tests/Makefile])])
    
AC_OUTPUT

# ---------------------------------------------------------------------------
# Output a summary of the results.
# ---------------------------------------------------------------------------
AS_IF([test "x$with_mpi" != xno],
  [networks="MPI"],
  [])
AS_IF([test "x$with_photon" != xno],
  [networks="$networks Photon"],
  [])
AS_IF([test "x$with_crypto" != xno],
  [hashing="$with_crypto"],
  [])
AS_IF([test "x$with_glib" != xno],
  [hashing="$with_glib"],
  [])

echo ""
echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "               Host CPU : $host_cpu"
echo "                Host OS : $host_os"
echo "         String Hashing : $hashing"
echo "               Networks : $networks"
echo "              Profiling : $with_papi"
echo "     Topology Awareness : $with_hwloc"
echo "  Thread-Caching Malloc : $with_tcmalloc"
echo "          Documentation : $enable_docs"
echo "             Test Suite : $enable_tests"
echo "================================================"

