
AC_INIT([hpx], [4.0.0a3], [hpx@indiana.edu])
AM_INIT_AUTOMAKE
AM_PROG_AR

LT_INIT

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC([icc gcc])
AM_PROG_CC_C_O
AM_PROG_AS

#AS_IF([test "x$CC" = "icc"], [CFLAGS += "-no-gcc"])

AC_ARG_WITH([papi], AS_HELP_STRING([--with-papi], [Build with PAPI measurement support]), [use_papi=yes], [0])

PKG_CHECK_MODULES([CHECK], [ check >= 0.9.4 ], [1], [0])

AS_IF([test "x$use_papi" = xyes], [
  PKG_CHECK_MODULES([PAPI], [papi])
  AC_DEFINE([WITH_PAPI], [], [Enable PAPI measurement support])
])

AM_CONDITIONAL([CPU_X86_64], [test "$host_cpu" = x86_64])

AM_CONDITIONAL([USE_PAPI], [test "x$use_papi" = xyes])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/libhpx/Makefile
  tests/Makefile
])
AC_OUTPUT