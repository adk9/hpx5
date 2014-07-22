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

AS_IF([test -z "$PE_ENV"],
 [AS_CASE([$CC],
   [craycc*], [AC_SUBST([PE_ENV],["CRAY"])],
     [pgcc*], [AC_SUBST([PE_ENV],["PGI"])],
      [icc*], [AC_SUBST([PE_ENV],["INTEL"])],
      [gcc*], [AC_SUBST([PE_ENV],["GNU"])],
              [AC_SUBST([PE_ENV],["UNKNOWN"])])])

cflags_save="$CFLAGS"
AC_MSG_CHECKING([for programming environment])
AS_CASE([$PE_ENV],
    [CRAY*],  [AS_IF([test -z "$CC"], [AC_SUBST([CC],["craycc"])])
              # ADK: We need this for ASM labels.
              CFLAGS="-hgnu"
              # Force static linking with craycc as it does not support
              # flags that LT adds for building a shared library.
              AC_DISABLE_SHARED
              AC_ENABLE_STATIC
              hpx_pe_env="CRAY"],
    [PGI*],   [AS_IF([test -z "$CC"], [AC_SUBST([CC],["pgcc"])])
              hpx_pe_env="PGI"],
    [INTEL*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["icc"])])
              hpx_pe_env="INTEL"],
    [GNU*],   [AS_IF([test -z "$CC"], [AC_SUBST([CC],["gcc"])])
              hpx_pe_env="GNU"],
    [hpx_pe_env="GNU"])
CFLAGS="$CFLAGS $cflags_save"
AC_MSG_RESULT([$hpx_pe_env])

AC_LANG([C])

AC_CONFIG_SRCDIR([include/config.h.in])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST

# Checks for programs.
cflags_save="$CFLAGS"
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AM_PROG_AS
CFLAGS="$cflags_save"

AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

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
AS_IF([test "x$enable_pedantic" != xno -a "x$hpx_pe_env" = xGNU],
  [AC_SUBST([HPX_PEDANTIC], [-pedantic])])

AC_ARG_ENABLE([wall],
  [AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@])],
  [], [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno -a "x$hpx_pe_env" = xGNU],
  [AC_SUBST([HPX_W_ALL], [-Wall])])

AC_ARG_ENABLE([werror],
  [AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=no@:>@])],
  [], [enable_werror=no])
AS_IF([test "x$enable_werror" != xno -a "x$hpx_pe_env" = xGNU],
  [AC_SUBST([HPX_W_ERROR], [-Werror])])

AC_SUBST([HPX_C99], [$ac_cv_prog_cc_c99])

AC_ARG_ENABLE([apps],
  [AS_HELP_STRING([--enable-apps], [Enable mini-apps @<:@default=no@:>@])],
  [], [enable_apps=no])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug code @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debugging stuff])])

AC_ARG_ENABLE([profiling],
  [AS_HELP_STRING([--enable-profiling], [Enable profiling support @<:@default=no@:>@])],
  [], [enable_profiling=no])
AS_IF([test "x$enable_profiling" != xno],
  [AC_DEFINE([ENABLE_PROFILING], [1], [Enable profiling support])])

AC_ARG_ENABLE([testsuite],
  [AS_HELP_STRING([--enable-testsuite], [Enable test suite (requires check) @<:@default=yes@:>@])],
  [], [enable_testsuite=yes])

# ---------------------------------------------------------------------------
# Openflow options.
# ---------------------------------------------------------------------------
HPX_WITH_PKG([trema],[libtrema],[use the Trema OF controller API],[no],[TREMA])
HPX_WITH_PKG([jansson],[jansson],[use libjansson for JSON parsing],[no],[JANSSON])
HPX_WITH_PKG([curl],[libcurl],[use libcurl for OF REST calls],[no],[CURL])
AC_ARG_ENABLE([floodlight],
  [AS_HELP_STRING([--enable-floodlight], [Enable Floodlight controller support @<:@default=no@:>@])],
  [], [enable_floodlight=no])
AS_IF([test "x$enable_floodlight" != xno],
  AS_IF([test "x$with_jansson" != xno -a "x$with_curl" != xno],
   [AC_DEFINE([HAVE_FLOODLIGHT],[1],[Enable Floodlight support])]))
AM_CONDITIONAL([HAVE_FLOODLIGHT], [test "xwith_jansson" != xno -a "x$with_curl" != xno])

# ---------------------------------------------------------------------------
# --with-options, see config/hpx_with_pkg.m4 for details
# ---------------------------------------------------------------------------
HPX_WITH_PKG([papi],[papi],[build with profiling],[no],[PAPI])
HPX_WITH_PKG([hwloc],[hwloc],[build with topology awareness],[no],[HWLOC])
HPX_WITH_PKG([pmi],[pmi],[build with support for PMI],[no],[PMI])

AC_CHECK_HEADER(pmi_cray_ext.h, AC_DEFINE([HAVE_PMI_CRAY_EXT], [1], [Enable Cray PMI extensions]))

HPX_WITH_PKG([mpi],[ompi],[build with MPI bootstrap],[no],[MPI])
HPX_WITH_PKG([photon],[photon],[build with Photon networking],[no],[PHOTON])
HPX_WITH_PKG([portals],[portals],[build with Portals networking],[no],[PORTALS])
AS_IF([test "x$with_mpi" != xno -o "x$with_photon" != xno -o "x$with_portals" != xno],
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
    [linux*],   [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
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
AM_CONDITIONAL([OS_LINUX], [[[[ "x$host_os" = xlinux* ]]]])
AM_CONDITIONAL([OS_DARWIN], [[[[ "x$host_os" = xdarwin* ]]]])
AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$with_photon" != xno -o "x$with_portals" != xno])
AM_CONDITIONAL([ENABLE_APPS], [test "x$enable_apps" != xno])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" != xno])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])

AM_CONDITIONAL([GNU_PE_ENV], [test "x$hpx_pe_env" = xGNU])
AM_CONDITIONAL([CRAY_PE_ENV], [test "x$hpx_pe_env" = xCRAY])
AM_CONDITIONAL([PGI_PE_ENV], [test "x$hpx_pe_env" = xPGI])
AM_CONDITIONAL([INTEL_PE_ENV], [test "x$hpx_pe_env" = xINTEL])

# -----------------------------------------------------------------------------
# Use autoconf to configure all of the Makefiles that need to be processed.
# -----------------------------------------------------------------------------
AC_CONFIG_FILES([
  Makefile
  libsync/Makefile
  libsync/arch/Makefile
  libhpx/Makefile
  libhpx/boot/Makefile
  libhpx/gas/Makefile
  libhpx/transport/Makefile
  libhpx/network/Makefile
  libhpx/system/Makefile
  libhpx/scheduler/Makefile
  libhpx/scheduler/arch/Makefile
  include/Makefile
  include/libhpx/Makefile
  examples/Makefile
])
# src/Makefile
# src/libhpx.pc

AS_IF([test "x$enable_apps" != xno],
  [AC_CONFIG_FILES([apps/Makefile
  apps/fmm/Makefile
  apps/fmm/parcels/Makefile
  apps/lulesh/Makefile
  apps/lulesh/parcels/Makefile
  apps/miniAMR/Makefile
  apps/miniAMR/parcels/Makefile])])
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
echo "================================================"

