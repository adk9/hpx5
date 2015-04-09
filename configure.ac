# Copyright (c) 2013-2015, Trustees of Indiana University 
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
AC_LANG([C])

AC_CONFIG_SRCDIR([include/config.h.in])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AX_COMPILER_VENDOR
HPX_PE_ENV

# Checks for programs.
AC_PROG_CC_C99
AC_PROG_INSTALL
AM_PROG_AS

# Initialize automake and libtool
AM_INIT_AUTOMAKE([1.11 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror parallel-tests color-tests])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.4.6])
LT_INIT()
#LT_INIT([disable-shared]) # we don't support dynamic linking correctly 

# Initialize pthreads
AX_PTHREAD([], [AC_MSG_ERROR([Could not find pthread implementation])])

# Check for parallel configuration
AC_ARG_ENABLE([parallel-config],
  [AS_HELP_STRING([--enable-parallel-config],
                  [Enable parallel configuration of contribs @<:@default=no@:>@])],
  [trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM], [enable_parallel_config=no])

# Initialize contribs
HPX_CONTRIB_UTHASH([contrib/uthash-fe01a6ad1b], [hpx_])
HPX_CONTRIB_LIBFFI([contrib/libffi], [hpx_])
HPX_CONTRIB_VALGRIND([contrib/valgrind], [hpx_])
HPX_CONTRIB_HWLOC([contrib/hwloc-1.10.0], [hpx_])

# Check if the assembler supports CFI directives
AX_CFI_DIRECTIVES

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

AC_ARG_ENABLE([apps],
  [AS_HELP_STRING([--enable-apps],
                  [Enable mini-apps @<:@default=no@:>@])],
  [], [enable_apps=no])

AC_ARG_ENABLE([tutorial],
  [AS_HELP_STRING([--enable-tutorial],
                  [Enable building of code samples from the HPX tutorial @<:@default=no@:>@])],
  [], [enable_tutorial=no])

AC_ARG_ENABLE([jemalloc],
  [AS_HELP_STRING([--enable-jemalloc],
                  [Enable using of jemalloc as the default allocator @<:@default=no@:>@])],
  [], [enable_jemalloc=no])
AS_IF([test "x$enable_jemalloc" != xno],
  [HPX_CONTRIB_JEMALLOC([contrib/jemalloc], [], [_local])
   AC_DEFINE([HAVE_JEMALLOC], [1], [We have jemalloc support for malloc/free/etc])]) 
  
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
                  [Enable debug code @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debugging stuff])])

AC_ARG_ENABLE([instrumentation],
  [AS_HELP_STRING([--enable-instrumentation],
                  [Enable instrumentation code @<:@default=no@:>@])],
  [], [enable_instrumentation=no])
AS_IF([test "x$enable_instrumentation" != xno],
  [AC_DEFINE([ENABLE_INSTRUMENTATION], [1], [Enable instrumentation for performance])])
AM_CONDITIONAL([ENABLE_INSTRUMENTATION], [test "x$enable_instrumentation" != xno])

AC_ARG_ENABLE([logging],
  [AS_HELP_STRING([--enable-logging],
                  [Enable logging code @<:@default=no@:>@])],
  [], [enable_logging=no])
AS_IF([test "x$enable_logging" != xno],
  [AC_DEFINE([ENABLE_LOGGING], [1], [Enable loggging stuff])])

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
                  [Enable test suite @<:@default=yes@:>@])],
  [], [enable_tests=yes])
  
AC_ARG_ENABLE([pwc],
  [AS_HELP_STRING([--enable-pwc],
                  [Enable the Put-With-Command (PWC) network @<:@default=yes@:>@])],
  [], [enable_pwc=yes])

AS_IF([test "x$enable_pwc" != xno],
  [AC_DEFINE([ENABLE_PWC], [1], [Enable pwc networking])])

AC_ARG_ENABLE([photon],
 [AS_HELP_STRING([--enable-photon],
                 [build with Photon networking @<:@default=no@:>@])],
  [], [enable_photon=no])

AS_IF([test "x$enable_photon" != xno],
 [HPX_CONTRIB_PHOTON([contrib/photon], [hpx_])
  HPX_CONTRIB_JEMALLOC([contrib/jemalloc], [libhpx_registered_], [_registered])
  AC_DEFINE([HAVE_JEMALLOC_REGISTERED], [1], [We have a registered memory manager])],
 [hpx_have_photon=no])

# ---------------------------------------------------------------------------
# --with-options, see config/hpx_with_pkg.m4 for details
# ---------------------------------------------------------------------------
HPX_WITH_PKG([pmi],[cray-pmi],[build with support for PMI],[no],[PMI])

AC_CHECK_HEADER(pmi_cray_ext.h, AC_DEFINE([HAVE_PMI_CRAY_EXT], [1], [Enable Cray PMI extensions]))
HPX_WITH_PKG([hugetlbfs],[hugetlbfs],[build with hugepages for heap],[no],[HUGETLBFS])
HPX_WITH_PKG([mpi],[ompi],[build with MPI bootstrap],[no],[MPI])
HPX_WITH_PKG([portals],[portals],[build with Portals networking],[no],[PORTALS])

AS_IF([test "x$with_mpi" != xno -o "x$enable_photon" != xno -o "x$with_portals" != xno],
 [enable_network=yes
  AC_DEFINE([HAVE_NETWORK], [1], [We have a network])
  AC_DEFINE([HAVE_JEMALLOC_GLOBAL], [1], [We have global allocation])
  HPX_CONTRIB_JEMALLOC([contrib/jemalloc], [libhpx_global_], [_global])
  TESTS_CMD='mpirun -np 2'],
 [enable_network=no
  TESTS_CMD=''])

AC_ARG_WITH([tests-cmd],
 [AS_HELP_STRING([--with-tests-cmd], [Use this command for running the test harness @<:@default=mpirun -np 2@:>@])],
 [TESTS_CMD="$with_tests_cmd"], [])
AC_SUBST(TESTS_CMD)

AC_ARG_WITH([doxygen],
 [AS_HELP_STRING([--with-doxygen], [build documentation @<:@default=no@:>@])],
 [with_doxygen=yes], [with_doxygen=no])
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
     [linux*], [AS_CASE([$host_cpu],
         [arm*], [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
             l1d_linesize=32
             pagesize=`getconf PAGESIZE`],
         [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
             dnl l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
             l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
             pagesize=`getconf PAGESIZE`])],
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
AM_CONDITIONAL([CPU_ARM], [test "x$host_cpu" = xarmv7l])

AM_CONDITIONAL([HAVE_PHOTON], [test "x$hpx_have_photon" != xno])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$enable_network" != xno])
AM_CONDITIONAL([ENABLE_PWC], [test "x$enable_pwc" != xno])
AM_CONDITIONAL([BUILD_JEMALLOC_GLOBAL], [test "x$enable_network" != xno])
AM_CONDITIONAL([BUILD_JEMALLOC_REGISTERED], [test "x$enable_photon" != xno])
AM_CONDITIONAL([BUILD_PHOTON], [test "x$enable_external_photon" == xno])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" != xno])
AM_CONDITIONAL([ENABLE_TUTORIAL], [test "x$enable_tutorial" != xno])
AM_CONDITIONAL([ENABLE_JEMALLOC], [test "x$enable_jemalloc" != xno])
AM_CONDITIONAL([ENABLE_PHOTON], [test "x$enable_photon" != xno])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])

AM_CONDITIONAL([GNU_PE_ENV], [test "x$hpx_pe_env" = xGNU])
AM_CONDITIONAL([CRAY_PE_ENV], [test "x$hpx_pe_env" = xCRAY])
AM_CONDITIONAL([PGI_PE_ENV], [test "x$hpx_pe_env" = xPGI])
AM_CONDITIONAL([INTEL_PE_ENV], [test "x$hpx_pe_env" = xINTEL])

# -----------------------------------------------------------------------------
# Finalize flags
# -----------------------------------------------------------------------------
hpx_extra_cppflags="$hpx_pe_env_cppflags $hpx_jemalloc_cppflags $HPX_PHOTON_CPPFLAGS $HPX_LIBFFI_CPPFLAGS $HPX_HWLOC_CPPFLAGS $hpx_valgrind_cppflags $hpx_uthash_cppflags"
hpx_extra_cflags="$hpx_pe_env_cflags $HPX_LIBFFI_CFLAGS $PTHREAD_CFLAGS $HPX_PHOTON_CFLAGS $hpx_valgrind_cflags $hpx_uthash_cflags"
hpx_extra_ccasflags="$hpx_pe_env_ccasflags"
hpx_extra_libs="\$(top_builddir)/libsync/libsync.la \$(top_builddir)/libhpx/libhpx.la $PTHREAD_LIBS $hpx_pe_env_libs"
AS_IF([test "x$enable_pedantic" != xno],
  [hpx_extra_cflags="$hpx_pe_env_cflags_pedantic $hpx_extra_cflags"])

AS_IF([test "x$enable_wall" != xno],
  [hpx_extra_cflags="$hpx_pe_env_cflags_wall $hpx_extra_cflags"])

CPPFLAGS="$hpx_extra_cppflags $CPPFLAGS"
CFLAGS="$hpx_extra_cflags $CFLAGS"
CCASFLAGS="$hpx_extra_ccasflags $CCASFLAGS"
HPX_LIBS="$hpx_extra_libs"
HPX_DEPS="\$(top_builddir)/libsync/libsync.la \$(top_builddir)/libhpx/libhpx.la $HPX_HWLOC_DEPS $HPX_PHOTON_DEPS $HPX_JEMALLOC_DEPS $HPX_LIBFFI_DEPS"
HPX_PRIVATE_PKGS=""
if test "x$enable_photon" != xno; then
  HPX_PRIVATE_PKGS="$HPX_PRIVATE_PKGS photon"
fi

AC_SUBST(HPX_LIBS)
AC_SUBST(HPX_DEPS)
AC_SUBST(HPX_PRIVATE_PKGS)

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
  libhpx/memory/Makefile
  libhpx/network/Makefile
  libhpx/network/isir/Makefile
  libhpx/network/pwc/Makefile
  libhpx/system/Makefile
  libhpx/util/Makefile
  libhpx/scheduler/Makefile
  libhpx/scheduler/arch/Makefile
  libhpx/instrumentation/Makefile
  include/Makefile
  include/libhpx/Makefile
  examples/Makefile
  hpx.pc])

AS_IF([test "x$enable_tutorial" != xno],
  [AC_CONFIG_FILES([tutorial/Makefile
  tutorial/Beginners/Makefile
  tutorial/Beginners/threads/Makefile
  tutorial/Beginners/gas/Makefile
  tutorial/Beginners/collectives/Makefile
  tutorial/Beginners/lco/Makefile
  tutorial/Beginners/parcel/Makefile
  tutorial/HeatSeq/Makefile
  tutorial/CollectiveBench/Makefile
  tutorial/cpi/Makefile])])
  
AS_IF([test "x$enable_docs" != xno],
  [AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])])
  
AS_IF([test "x$enable_tests" != xno],
  [AC_CONFIG_FILES([tests/Makefile
  tests/unit/Makefile
  tests/perf/Makefile])])
  
AC_OUTPUT

if test "x$enable_parallel_config" != xno; then
   echo "*** Waiting for contrib's configuration to finish..."
   wait
fi

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
echo "               jemalloc : $enable_jemalloc"
echo "              Profiling : $enable_profiling"
echo "         hugetlbfs heap : $with_hugetlbfs"
echo "     Topology Awareness : $with_hwloc"
echo "          Documentation : $enable_docs"
echo "               Tutorial : $enable_tutorial"
echo "             Test Suite : $enable_tests"
echo "         Extra CPPFLAGS : $hpx_extra_cppflags"
echo "           Extra CFLAGS : $hpx_extra_cflags"
echo "        Extra CCASFLAGS : $hpx_extra_ccasflags"
echo "       Install location : $prefix"
echo "================================================"
