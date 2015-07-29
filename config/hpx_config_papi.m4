# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_PAPI([pkg-config])
# ------------------------------------------------------------------------------
#
# Variables
#   enable_papi
#   with_papi
#   have_papi
#
# Appends
#   LIBHPX_CFLAGS
#   LIBHPX_LIBS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_PAPI
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_PAPI], [
 AC_DEFINE([HAVE_PAPI], [1], [Have support for papi])
 have_papi=yes
])

AC_DEFUN([_HPX_CC_PAPI], [
  # check and see if papi is "just available" without any work
  AC_CHECK_HEADER([papi.h],
    [AC_CHECK_LIB([papi], [PAPI_library_init],
      [_HAVE_PAPI
       LIBHPX_LIBS="$LIBHPX_LIBS -lpapi"
       HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lpapi"])])
])

AC_DEFUN([_HPX_PKG_PAPI], [pkg=$1
 # check if we need to use a papi package to access the functionality
 PKG_CHECK_MODULES([PAPI], [$pkg],
   [_HAVE_PAPI
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $PAPI_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $PAPI_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"])
])

AC_DEFUN([_HPX_WITH_PAPI], [pkg=$1
 #
 AS_CASE($with_papi,
   [no], [AC_MSG_ERROR([--enable-papi=$enable_papi excludes --without-papi])],
   [system], [_HPX_CC_PAPI
              AS_IF([test "x$have_papi" != xyes], [_HPX_PKG_PAPI($pkg)])],
   [yes], [_HPX_CC_PAPI
           AS_IF([test "x$have_papi" != xyes], [_HPX_PKG_PAPI($pkg)])],
   [_HPX_PKG_PAPI($with_papi)])
])

AC_DEFUN([HPX_CONFIG_PAPI], [pkg=$1
 # allow the programmer to select to use papi support in the gas allocation
 AC_ARG_ENABLE([papi],
   [AS_HELP_STRING([--enable-papi],
                   [Enable papi support @<:@default=no@:>@])],
   [], [enable_papi=no])

 AC_ARG_WITH(papi,
   [AS_HELP_STRING([--with-papi{=system,PKG}],
                   [How we find PAPI @<:@default=system,$pkg@:>@])],
   [], [with_papi=system])

 AS_IF([test "x$enable_papi" != xno], [_HPX_WITH_PAPI($pkg)])
])
