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

dnl Allow users to build libitm with warnings treated as errors (disabled by
dnl default). 
AC_ARG_ENABLE([pedantic],
    AS_HELP_STRING([--enable-pedantic], [Enable -pedantic @<:@default=yes@:>@]),
    [],
    [enable_pedantic=yes])
AS_IF([test "x$enable_pedantic" != xno], AC_SUBST([HPX_PEDANTIC], [-pedantic]))

dnl Allow users to build libitm with warnings treated as errors (disabled by
dnl default). 
AC_ARG_ENABLE([wall],
    AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@]),
    [],
    [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno], AC_SUBST([HPX_W_ALL], [-Wall]))

dnl Allow users to build libitm with warnings treated as errors (disabled by
dnl default). 
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=yes@:>@]),
    [],
    [enable_werror=yes])
AS_IF([test "x$enable_werror" != xno], AC_SUBST([HPX_W_ERROR], [-Werror]))

dnl Allow users to build libitm with link-time-optimization.
AC_ARG_ENABLE([lto],
    AS_HELP_STRING([--enable-lto],
                   [Enable link-time optimization @<:@default=no@:>@]),
    [],
    [enable_lto=no])
AS_IF([test "x$enable_lto" != xno], AC_SUBST([HPX_F_LTO], [-flto]))

dnl Use md5 from libcrypto to hash action string keys
AC_ARG_WITH([openssl],
    AS_HELP_STRING([--with-openssl],
                   [Build with openssl for string hashing @<:@default=yes@:>@]),
    [],
    [use_openssl=yes])
AS_IF([test "x$use_openssl" != xno], [
  PKG_CHECK_MODULES([OPENSSL], [openssl])
  AC_DEFINE([WITH_OPENSSL], [], [Enable openssl for string hashing])
  AC_SUBST(OPENSSL_CFLAGS)
  AC_SUBST(OPENSSL_LIBS)
])

AC_SUBST(PAPI_CFLAGS)
AC_SUBST(PAPI_LIBS)

# Check for PAPI measurement support.
AC_ARG_WITH([papi],
    [AS_HELP_STRING([--with-papi],
                    [Build with PAPI measurement support @<:@default=no@:>@])],
    [],
    [use_papi=no])
AS_IF([test "x$use_papi" != xno], [
  PKG_CHECK_MODULES([PAPI], [papi])
  AC_DEFINE([WITH_PAPI], [], [Enable PAPI measurement support])
  AC_SUBST(PAPI_CFLAGS)
  AC_SUBST(PAPI_LIBS)
])

# Check for HWLOC.
AC_ARG_WITH([hwloc],
     [AS_HELP_STRING([--with-hwloc],
                     [Build with HWLOC support @<:@default=no@:>@])],
     [],
     [use_hwloc=no])
AS_IF([test "x$use_hwloc" != xno], [
  PKG_CHECK_MODULES([HWLOC], [hwloc])
  AC_DEFINE([WITH_HWLOC], [], [Enable HWLOC support])
  AC_SUBST(HWLOC_CFLAGS)
  AC_SUBST(HWLOC_LIBS)
])

# Check for TCMALLOC.
AC_ARG_WITH([tcmalloc],
    [AS_HELP_STRING([--with-tcmalloc],
                    [Build with TCMALLOC support @<:@default=no@:>@])],
    [],
    [use_tcmalloc=no])
AS_IF([test "x$use_tcmalloc" != xno], [
  PKG_CHECK_MODULES([TCMALLOC], [libtcmalloc])
  AC_DEFINE([WITH_TCMALLOC], [], [Enable TCMALLOC support])
  AC_SUBST(TCMALLOC_CFLAGS)
  AC_SUBST(TCMALLOC_LIBS)
])

# Check for MPI
AC_ARG_WITH([mpi],
    [AS_HELP_STRING([--with-mpi[[=PKGS]]],
                    [Build with MPI support @<:@default=yes@:>@])],
    [],
    [with_mpi=yes])
AS_IF([test "x$with_mpi" != xno],
      [AC_DEFINE([WITH_MPI], [], [Enable MPI support])
       AC_MSG_NOTICE([asking for mpi $with_mpi])
       AS_IF([test "x$with_mpi" != xyes],
             [PKG_CHECK_MODULES([MPI], $with_mpi)
              AC_SUBST(MPI_CFLAGS)
              AC_SUBST(MPI_LIBS)])])

# Check for PHOTON
AC_ARG_WITH([photon],
    [AS_HELP_STRING([--with-photon],
                    [Build with Photon support @<:@default=no@:>@])],
    [],
    [use_photon=no])
AS_IF([test "x$use_photon" != xno], [
  AC_DEFINE([WITH_PHOTON], [], [Enable Photon support])
])

# Check for check (and disable test-suite if not found).
PKG_CHECK_MODULES([CHECK], [ check >= 0.9.4 ], [
  AC_DEFINE([WITH_TESTSUITE], [], [Enable test suite])
  AM_CONDITIONAL([WITH_TESTSUITE], true)
  AC_SUBST(CHECK, [check])
  AC_SUBST(CHECK_CFLAGS)
  AC_SUBST(CHECK_LIBS)], [
  AC_MSG_WARN([Check not found. Install Check to enable the test-suite.])
  AM_CONDITIONAL([WITH_TESTSUITE], false)])

#dnl AM_CONF_IF macro is too recent for Cray
#AM_COND_IF([WITH_TESTSUITE],
#       [AC_CONFIG_FILES([tests/Makefile])])
if test -n "$CHECK"; then
   AC_CONFIG_FILES([tests/Makefile])
fi

# Check for doxygen (to build the documentation).
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
  then AC_MSG_WARN([Doxygen not found. Install Doxygen to build the documentation.])
fi

AM_CONDITIONAL([WITH_DOCS], [test -n "$DOXYGEN"])
#AM_COND_IF([WITH_DOCS],
#       [AC_CONFIG_FILES([docs/Doxyfile
#                         docs/Makefile])])
if test -n "$DOXYGEN"; then
   AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])
fi
AC_SUBST(WITH_DOCS)

AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([PLATFORM_LINUX], [test "x$host_os" = xlinux-gnu])
AM_CONDITIONAL([HAVE_PAPI], [test "x$use_papi" != xno])
AM_CONDITIONAL([HAVE_HWLOC], [test "x$use_hwloc" != xno])
AM_CONDITIONAL([HAVE_TCMALLOC], [test "x$use_tcmalloc" != xno])
AM_CONDITIONAL([HAVE_MPI], [test "x$with_mpi" != xno])
AM_CONDITIONAL([HAVE_PHOTON], [test "x$use_photon" != xno])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$use_photon" != xno]) # TODO: make this robust

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  include/Makefile
  examples/Makefile
  examples/hpx/Makefile	
  examples/thread/Makefile
  examples/network/Makefile
])
AC_OUTPUT

AS_IF([test -n "$DOXYGEN"],
      [doc_string="yes"],
      [doc_string="no"])
AS_IF([test -n "$CHECK"],
      [test_string="yes"],
      [test_string="no"])
AS_IF([test "x$use_papi" != xno],
      [papi_string="yes"],
      [papi_string="no"])
AS_IF([test "x$use_hwloc" != xno],
      [hwloc_string="yes"],
      [hwloc_string="no"])
AS_IF([test "x$use_tcmalloc" != xno],
      [tcmalloc_string="yes"],
      [tcmalloc_string="no"])
AS_IF([test "x$with_mpi" != xno],
      [networks_string="$networks_string MPI"],
      [])
AS_IF([test "x$use_photon" != xno],
      [networks_string="$networks_string Photon"],
      [])
AS_IF([test "x$use_openssl" != xno],
      [openssl_string="OpenSSL libcrypto"],
      [openssl_string="builtin"])

echo ""
echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "               Host CPU : $host_cpu"
echo "                Host OS : $host_os"
echo "         String Hashing : $openssl_string"
echo "               Networks : $networks_string"
echo "              Profiling : $papi_string (PAPI)"
echo "     Topology Awareness : $hwloc_string (HWLOC)"
echo "  Thread-Caching Malloc : $tcmalloc_string (TCMALLOC)"
echo "          Documentation : $doc_string"
echo "             Test Suite : $test_string"
echo "================================================"

