# Copyright (c) 2013, Trustees of Indiana University 
# All rights reserved.

# This software may be modified and distributed under the terms of
# the BSD license.  See the COPYING file for details.

# This software was created at the Indiana University Center for
# Research in Extreme Scale Technologies (CREST).

AC_PREREQ([2.69])
AC_INIT([hpx], [4.0.0a4], [hpx@indiana.edu])
CLEANFILES="*~ .\#* .gdb_history"
AC_SUBST(CLEANFILES)

AC_CONFIG_SRCDIR([include/hpx/list.h])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar parallel-tests -Wall -Werror])
AM_PROG_AR
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
AC_CHECK_HEADERS([limits.h mach/mach.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
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

PKG_CHECK_MODULES([CHECK], [ check >= 0.9.4 ])

AC_ARG_WITH([papi], AS_HELP_STRING([--with-papi], [Build with PAPI measurement support]), [use_papi=yes], [])
AS_IF([test "x$use_papi" = xyes], [
  PKG_CHECK_MODULES([PAPI], [papi])
  AC_DEFINE([WITH_PAPI], [], [Enable PAPI measurement support])
])

AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
  then AC_MSG_WARN([Doxygen not found. Install Doxygen to build the documentation.])
fi

AM_CONDITIONAL([CPU_X86_64], [test "$host_cpu" = x86_64])
AM_CONDITIONAL([PLATFORM_LINUX], [test "$host_os" = linux-gnu])
AM_CONDITIONAL([HAVE_PAPI], [test "x$use_papi" = xyes])

AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "x$DOXYGEN"])
AM_COND_IF([HAVE_DOXYGEN],
           [AC_CONFIG_FILES([docs/Doxyfile
		             docs/Makefile])])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  examples/Makefile
  tests/Makefile
])
AC_OUTPUT

AS_IF([test -n "x$DOXYGEN"],
      [doc_string="yes"],
      [doc_string="no"])
AS_IF([test "x$use_papi" = xyes],
      [papi_string="yes"],
      [papi_string="no"])

echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "              Host CPU  : $host_cpu"
echo "               Host OS  : $host_os"
echo "             Profiling  : $papi_string"
echo "          Documentation : $doc_string"
echo "================================================"

