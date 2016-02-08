# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_TASKSCALES([path])
#
# Variables
#   with_taskscales
#   have_taskscales
#
# Appends
#   LIBHPX_CFLAGS
#   LIBHPX_LIBS
#   HPX_PC_CFLAGS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_TASKSCALES
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_TASKSCALES], [
 AC_DEFINE([HAVE_TASKSCALES], [1], [We have taskscales support])
 have_taskscales=yes
])

# Check to see if there is a version of taskscales in the path without requiring any
# modifications. In this case, we need to add the taskscales library so that we can
# link to it and make sure that any pkg-config consumers know we have taskscales.
AC_DEFUN([_HPX_CC_TASKSCALES], [
 AC_LANG(C++)
 SAVED_LDFLAGS=$LDFLAGS
 LDFLAGS="$LDFLAGS -lTaskScales"
 AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
 AC_CHECK_HEADER([TaskScales/core.h],
   [AC_LINK_IFELSE([
     AC_LANG_PROGRAM([#include <TaskScales/core.h>],
      [TaskScales::version()])],
     [_HAVE_TASKSCALES
      LIBHPX_LIBS="$LIBHPX_LIBS -lTaskScales"
      HPX_PC_CFLAGS="$HPX_PC_CFLAGS -DHAVE_TASKSCALES"
      HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lTaskScales"])])
 LDFLAGS=$SAVED_LDFLAGS
])

# Use pkg-config to find an taskscales module.
AC_DEFUN([_HPX_PKG_TASKSCALES], [
 pkg=$1 

 PKG_CHECK_MODULES([TASKSCALES], [$pkg],
   [_HAVE_TASKSCALES
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $TASKSCALES_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $TASKSCALES_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"])
])

# Process the --with-taskscales option.
AC_DEFUN([_HPX_WITH_TASKSCALES], [
 pkg=$1

 AS_CASE($with_taskscales,
   # yes and system are equivalent---we first see if taskscales is available in the
   # user's path, and then check to see if the default module us available. I
   # don't know how to fallthrough-case with AS_CASE, so we have the identical
   # branches. 
   [yes],    [_HPX_CC_TASKSCALES
              AS_IF([test "x$have_taskscales" != xyes], [_HPX_PKG_TASKSCALES($pkg)])],  
   [system], [_HPX_CC_TASKSCALES
              AS_IF([test "x$have_taskscales" != xyes], [_HPX_PKG_TASKSCALES($pkg)])],

   # any other string is interpreted as a custom package name for pkg-config 
   [_HPX_PKG_TASKSCALES($with_taskscales)])
])

AC_DEFUN([HPX_CONFIG_TASKSCALES], [
 pkg=$1
 wanted=$2

 AC_ARG_WITH(taskscales,
   [AS_HELP_STRING([--with-taskscales{=system,PKG}],
                   [How we find the taskscales library @<:@default=system,$pkg@:>@])],
   [], [with_taskscales=system])
 
 AS_IF([test "x$wanted" != xno], [_HPX_WITH_TASKSCALES($pkg)])
])
