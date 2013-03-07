
AC_INIT([hpx], [4.0.0a1], [hpx@indiana.edu])
AM_INIT_AUTOMAKE
AM_PROG_AR

LT_INIT

AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AS

PKG_CHECK_MODULES([CHECK], [ check >= 0.9.4 ])

AM_CONDITIONAL([CPU_X86_64], [test "$host_cpu" = x86_64])

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/libhpx/Makefile
  tests/Makefile
])
AC_OUTPUT