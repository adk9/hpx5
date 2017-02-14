# -*- autoconf -*---------------------------------------------------------------# HPX_CONFIG_CUDA([pkg])
# ------------------------------------------------------------------------------
#
# Variables
#   enable_cuda
#   with_cuda
#   have_cuda
#
# Appends
#   LIBHPX_CFLAGS
#   HPX_APPS_LDADD
#   HPX_APPS_CFLAGS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_CUDA
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_CUDA], [
 AC_DEFINE([HAVE_CUDA], [1], [Have support for cuda])
 have_cuda=yes
])

AC_DEFUN([_HPX_CC_CUDA], [
  # check and see if cuda is "just available" without any work
  AC_MSG_CHECKING([for direct CC support for CUDA])
  AC_LANG_PUSH([C])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[#include <cuda.h>
                      #include <cuda_runtime.h>
                    ]],
                    [[void* ptr = 0;cudaMalloc(&ptr, 1);]])], 
    [AC_MSG_RESULT(yes)
     _HAVE_CUDA],
    [AC_MSG_RESULT(no)])
  AC_LANG_POP([C])
])

AC_DEFUN([_HPX_LIB_CUDA], [
  # look for libcuda in the path, this differs from _HPX_CC_CUDA in the sense
  # that it is testing -lcuda.
  AC_CHECK_HEADER([cuda.h],
    [AC_CHECK_LIB([cuda], [cuInit],
      [_HAVE_CUDA
       HPX_APPS_LDADD="$HPX_APPS_LDADD -lcuda"
       HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lcuda"])])
])

AC_DEFUN([_HPX_PKG_CUDA], [pkg=$1
 # check if we need to use a cuda package to access the functionality
 PKG_CHECK_MODULES([cuda], [$pkg],
   [_HAVE_CUDA
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $CUDA_CFLAGS"
    LIBHPX_CXXFLAGS="$LIBHOX_CXXFLAGS $CUDA_CXXFLAGS"
    HPX_APPS_CFLAGS="$HPX_APPS_CFLAGS $CUDA_CFLAGS"
    HPX_APPS_CXXFLAGS="$HPX_APPS_CXXFLAGS $CUDA_CFLAGS"
    HPX_APPS_LDADD="$HPX_APPS_LDADD $CUDA_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"])
])

AC_DEFUN([_HPX_WITH_CUDA], [pkg=$1
 # handle the with_cuda option if CUDA is enabled
 AS_CASE($with_cuda,
   [no],     [AC_MSG_ERROR([--enable-cuda=$enable_cuda excludes --without-cuda])],
   [system], [_HPX_CC_CUDA
              AS_IF([test "x$have_cuda" != xyes], [_HPX_LIB_CUDA])
              AS_IF([test "x$have_cuda" != xyes], [_HPX_PKG_CUDA($pkg)])],
   [yes],    [_HPX_CC_CUDA
              AS_IF([test "x$have_cuda" != xyes], [_HPX_LIB_CUDA])
              AS_IF([test "x$have_cuda" != xyes], [_HPX_PKG_CUDA($pkg)])],
   [_HPX_PKG_CUDA($with_cuda)])
])

AC_DEFUN([HPX_CONFIG_CUDA], [
 pkg=$1

 # Select if we want to support cuda, and how to find it if we do.
 AC_ARG_ENABLE([cuda],
   [AS_HELP_STRING([--enable-cuda],
                   [Enable CUDA support (network) @<:@default=no@:>@])],
   [], [enable_cuda=no])

 # allow the programmer to select to use cuda support
 AC_ARG_WITH(cuda,
   [AS_HELP_STRING([--with-cuda{=system,PKG}],
                   [How we find cuda @<:@default=system,$pkg@:>@])],
   [], [with_cuda=system])

 AS_IF([test "x$enable_cuda" != xno], [_HPX_WITH_CUDA(pkg)])
])
