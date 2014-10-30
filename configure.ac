# Copyright (c) 2013, Trustees of Indiana University 
# All rights reserved.

# This software may be modified and distributed under the terms of
# the BSD license.  See the COPYING file for details.

# This software was created at the Indiana University Center for
# Research in Extreme Scale Technologies (CREST).

AC_PREREQ([2.63])
AC_INIT([hpx], [5.0.0a1], [hpx@indiana.edu])
CLEANFILES="*~ .\#*"
AC_SUBST(CLEANFILES)

# ---------------------------------------------------------------------------
# Detect the programming environment.
# ---------------------------------------------------------------------------
HPX_PE_ENV

AC_LANG([C])

AC_CONFIG_SRCDIR([include/config.h.in])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AM_PROG_AS

# Initialize automake and libtool
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT([disable-shared]) # we don't support dynamic linking correctly yet

# Initialize pthreads
AX_PTHREAD([], [AC_MSG_ERROR([Could not find pthread implementation])])

# intialize contribs
HPX_CONTRIB_HWLOC([contrib/hwloc-1.9.1], [hpx_])
HPX_CONTRIB_UTHASH([contrib/uthash-fe01a6ad1b], [hpx_])
HPX_CONTRIB_JEMALLOC([contrib/jemalloc], [hpx_])
HPX_CONTRIB_VALGRIND([contrib/valgrind], [hpx_])

# NB: Don't check for anything that we're not actually prepared to deal with
#     in our source code---this just clutters the configure and gives users a
#     false sense of security.

# ---------------------------------------------------------------------------
# --enable options
# ---------------------------------------------------------------------------
AC_ARG_ENABLE([pedantic],
  [AS_HELP_STRING([--enable-pedantic],
                  [Enable pedantic C99 @<:@default=yes@:>@])],
  [], [enable_pedantic=yes])

AC_ARG_ENABLE([wall],
  [AS_HELP_STRING([--enable-wall],
                  [Enable all warnings @<:@default=yes@:>@])],
  [], [enable_wall=yes])

AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror],
                  [Treat warnings as errors @<:@default=no@:>@])],
  [], [enable_werror=no])

AC_ARG_ENABLE([apps],
  [AS_HELP_STRING([--enable-apps],
                  [Enable mini-apps @<:@default=no@:>@])],
  [], [enable_apps=no])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
                  [Enable debug code @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debugging stuff])])

AC_ARG_ENABLE([photon-debug],
  [AS_HELP_STRING([--enable-photon-debug],
                  [Enable photon debugging @<:@default=no@:>@])],
  [], [enable_photon_debug=no])
AS_IF([test "x$enable_photon_debug" != xno],
  [AC_DEFINE([ENABLE_PHOTON_DEBUG], [1], [Enable photon debugging stuff])])
AM_CONDITIONAL([ENABLE_PHOTON_DEBUG], [test "$enable_photon_debug" = "yes"])

AC_ARG_ENABLE([photon-trace],
  [AS_HELP_STRING([--enable-photon-trace],
                  [Enable photon call tracing @<:@default=no@:>@])],
  [], [enable_photon_trace=no])
AS_IF([test "x$enable_photon_trace" != xno],
  [AC_DEFINE([ENABLE_PHOTON_TRACE], [1], [Enable photon call tracing (files)])])
AM_CONDITIONAL([ENABLE_PHOTON_TRACE], [test "$enable_photon_trace" = "yes"])

AC_ARG_ENABLE([profiling],
  [AS_HELP_STRING([--enable-profiling],
                  [Enable profiling support @<:@default=no@:>@])],
  [], [enable_profiling=no])
AS_IF([test "x$enable_profiling" != xno],
  [AC_DEFINE([ENABLE_PROFILING], [1], [Enable profiling support])])

AC_ARG_ENABLE([lengthy-tests],
    [AC_HELP_STRING([--enable-lengthy-tests],
                    [Enable long running tests in the test suite (default: disabled)])])
AM_CONDITIONAL([ENABLE_LENGTHY_TESTS], [test "$enable_lengthy_tests" = "yes"])

AC_ARG_ENABLE([testsuite],
  [AS_HELP_STRING([--enable-testsuite],
                  [Enable test suite (requires check) @<:@default=yes@:>@])],
  [], [enable_testsuite=yes])

HPX_OPENFLOW

# ---------------------------------------------------------------------------
# --with-options, see config/hpx_with_pkg.m4 for details
# ---------------------------------------------------------------------------
HPX_WITH_PKG([papi],[papi],[build with profiling],[no],[PAPI])
HPX_WITH_PKG([pmi],[pmi],[build with support for PMI],[no],[PMI])

AC_CHECK_HEADER(pmi_cray_ext.h, AC_DEFINE([HAVE_PMI_CRAY_EXT], [1], [Enable Cray PMI extensions]))

HPX_WITH_PKG([mpi],[ompi],[build with MPI bootstrap],[no],[MPI])
HPX_WITH_PKG([portals],[portals],[build with Portals networking],[no],[PORTALS])

HPX_WITH_VERBS
HPX_WITH_PKG([ugni],[ugni],[Build with UGNI],[no],[UGNI])

AM_CONDITIONAL([HAVE_XSP], [false])
AM_CONDITIONAL([PHOTON_ENABLE_TESTS], [false])

AC_ARG_ENABLE([photon],
 [AS_HELP_STRING([--enable-photon], [build with Photon networking @<:@default=no@:>@])], [],
 [enable_photon=no])
AS_IF([test "x$enable_photon" != xno],
  [HPX_CONTRIB_PHOTON([contrib/photon], [hpx_])],
  [hpx_have_photon=no])
AM_CONDITIONAL([HAVE_PHOTON], [test "x$hpx_have_photon" != xno])
AM_CONDITIONAL([BUILD_PHOTON], [test "x$hpx_have_photon" == xinternal])

AS_IF([test "x$with_mpi" != xno -o "x$enable_photon" != xno -o "x$with_portals" != xno],
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
      [linux*], [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
                 dnl l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
                 l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
                 pagesize=`getconf PAGESIZE`],
     [darwin*], [l1d_linesize=`sysctl -n hw.cachelinesize`
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
AM_CONDITIONAL([OS_LINUX], [[[[ "x$host_os" = xlinux* ]]]])
AM_CONDITIONAL([OS_DARWIN], [[[[ "x$host_os" = xdarwin* ]]]])
AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$enable_photon" != xno -o "x$with_portals" != xno])
AM_CONDITIONAL([ENABLE_APPS], [test "x$enable_apps" != xno])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" != xno])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])

AM_CONDITIONAL([GNU_PE_ENV], [test "x$hpx_pe_env" = xGNU])
AM_CONDITIONAL([CRAY_PE_ENV], [test "x$hpx_pe_env" = xCRAY])
AM_CONDITIONAL([PGI_PE_ENV], [test "x$hpx_pe_env" = xPGI])
AM_CONDITIONAL([INTEL_PE_ENV], [test "x$hpx_pe_env" = xINTEL])

# -----------------------------------------------------------------------------
# Finalize flags
# -----------------------------------------------------------------------------
hpx_extra_cppflags="$hpx_pe_env_cppflags $HPX_JEMALLOC_CPPFLAGS $HPX_PHOTON_CPPFLAGS $hpx_valgrind_cppflags $hpx_uthash_cppflags"
hpx_extra_cflags="$hpx_pe_env_cflags $PTHREAD_CFLAGS"
hpx_extra_ccasflags="$hpx_pe_env_ccasflags"
hpx_extra_libs="$PTHREAD_LIBS $HPX_PHOTON_LIBS"
hpx_extra_ldflags="$HPX_PHOTON_LDFLAGS"
AS_IF([test "x$enable_pedantic" != xno],
  [hpx_extra_cflags="$hpx_pe_env_cflags_pedantic $hpx_extra_cflags"])

AS_IF([test "x$enable_wall" != xno],
  [hpx_extra_cflags="$hpx_pe_env_cflags_wall $hpx_extra_cflags"])
  
AS_IF([test "x$enable_werror" != xno],
  [hpx_extra_cflags="$hpx_pe_env_cflags_werror $hpx_extra_cflags"])

CPPFLAGS="$hpx_extra_cppflags $CPPFLAGS"
CFLAGS="$hpx_extra_cflags $CFLAGS"
CCASFLAGS="$hpx_extra_ccasflags $CCASFLAGS"
LIBS="$hpx_extra_libs $LIBS"
LDFLAGS="$hpx_extra_ldflags $LDFLAGS"

# -----------------------------------------------------------------------------
# Use autoconf to configure all of the Makefiles that need to be processed.
# -----------------------------------------------------------------------------
AC_CONFIG_FILES([
  Makefile
  contrib/Makefile
  libsync/Makefile
  libsync/arch/Makefile
  libhpx/Makefile
  libhpx/boot/Makefile
  libhpx/gas/Makefile
  libhpx/transport/Makefile
  libhpx/network/Makefile
  libhpx/system/Makefile
  libhpx/util/Makefile
  libhpx/scheduler/Makefile
  libhpx/scheduler/arch/Makefile
  include/Makefile
  include/libhpx/Makefile
  examples/Makefile
  bench/Makefile])

AS_IF([test "x$enable_apps" != xno],
  [AC_CONFIG_FILES([apps/Makefile
  apps/mpi/Makefile
  apps/fmm/Makefile
  apps/fmm/parcels/Makefile
  apps/lulesh/Makefile
  apps/lulesh/parcels/Makefile
  apps/lulesh/newfutures/Makefile
  apps/miniAMR/Makefile
  apps/miniAMR/parcels/Makefile
  apps/libPXGL/Makefile
  apps/libPXGL/examples/Makefile])])
  
AS_IF([test "x$enable_docs" != xno],
  [AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])])
  
AS_IF([test "x$enable_tests" != xno],
  [AC_CONFIG_FILES([tests/Makefile
  tests/unit/Makefile
  tests/performance/Makefile
  tests/performance/TaskSpawningSO/Makefile
  tests/performance/MicroBench/Makefile
  tests/performance/Overheads/Makefile
  tests/performance/PutGetSM/Makefile])])
  
AC_OUTPUT

# ---------------------------------------------------------------------------
# Output a summary of the results.
# ---------------------------------------------------------------------------
AS_IF([test "x$with_mpi" != xno],
  [networks="MPI"],
  [])
AS_IF([test "x$enable_photon" != xno],
  [networks="Photon $networks"],
  [])
AS_IF([test "x$with_portals" != xno],
  [networks="Portals $networks"],
  [])

echo ""
echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "               Host CPU : $host_cpu"
echo "                Host OS : $host_os"
echo "               Networks : $networks"
echo "              Profiling : $with_papi"
echo "     Topology Awareness : $with_hwloc"
echo "            SDN support : $with_trema"
echo "          Documentation : $enable_docs"
echo "             Test Suite : $enable_tests"
echo "         Extra CPPFLAGS : $hpx_extra_cppflags"
echo "           Extra CFLAGS : $hpx_extra_cflags"
echo "        Extra CCASFLAGS : $hpx_extra_ccasflags"
echo "================================================"

