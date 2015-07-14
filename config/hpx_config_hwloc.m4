# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_HWLOC([path])
#
# HPX will use the hwloc library to deal with node-local topology. This
# functionality will always be optional, and can be controled with the
# --enable-hwloc option. We prefer our contrib hwloc because it can be built
# into libhpx as an embedded option, but the --with-hwloc option can override
# this behavior.
#
# Variables
#   enable_hwloc
#   with_hwloc
#   have_hwloc
#   build_hwloc
#
# Substitutes
#   LIBHPX_CPPFLAGS
#   LIBHPX_CFLAGS
#   LIBHPX_LIBADD
#   LIBHPX_LIBS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_HWLOC
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_HWLOC], [
 AC_DEFINE([HAVE_HWLOC], [1], [We have hwloc support])
 have_hwloc=yes
])

AC_DEFUN([_HPX_CONTRIB_HWLOC], [
 contrib=$1
 
 # Disable features that are not required for our embedded build
 enable_libxml2=no
 enable_libnuma=no
 enable_pci=no
 enable_opencl=no
 enable_cuda=no
 enable_nvml=no
 enable_gl=no
 HWLOC_SETUP_CORE([$contrib], [_HAVE_HWLOC],
   [AC_MSG_WARN([could not configure hwloc])],
   [1])
 unset enable_libxml2
 unset enable_libnuma
 unset enable_pci
 unset enable_opencl
 unset enable_cuda
 unset enable_nvml
 unset enable_gl

 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS $HWLOC_EMBEDDED_CPPFLAGS"
 LIBHPX_LIBADD="$LIBHPX_LIBADD $HWLOC_EMBEDDED_LDADD"
])

AC_DEFUN([_HPX_PKG_HWLOC], [
 pkg=$1
 
 # use pkg-config to find an hwloc module
 PKG_CHECK_MODULES([HWLOC], [$pkg],
   [_HAVE_HWLOC
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $HWLOC_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $HWLOC_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"])
])

AC_DEFUN([_HPX_LIB_HWLOC], [
 # check to see if there is a system-installed version of hwloc
 AC_CHECK_HEADER([hwloc.h],
   [AC_CHECK_LIB([hwloc], [hwloc_topology_init],
     [_HAVE_HWLOC
      LIBHPX_LIBS="$LIBHPX_LIBS -lhwloc"
      HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lhwloc"])])
])

AC_DEFUN([_HPX_WITH_HWLOC], [
 pkg=$1
 
 # handle with with_hwloc option, when enable_hwloc is set
 AS_CASE($with_hwloc,

   # no means the user has supplied --without-hwloc, or --with-hwloc=no. This is
   # inconsistent with selecting --enable-hwloc, which is the only way we get to
   # this point, so we can generate an error
   [no], [AC_MSG_ERROR([--enable-hwloc=$enable_hwloc excludes --without-hwloc])],

   # contrib means that we should just go ahead and configure and use the
   # contributed hwloc library
   [contrib], [build_hwloc=yes],
   [yes], [build_hwloc=yes],
   
   # system indicates that we should look for a system-installed hwloc, first in
   # the current path and then a default-named pkg-config package.
   [system], [_HPX_LIB_HWLOC
              AS_IF([test "x$have_hwloc" != xyes], [_HPX_PKG_HWLOC($pkg)])],

   # any other string is interpreted as a custom package name for pkg-config 
   [_HPX_PKG_HWLOC($with_hwloc)])
])

AC_DEFUN([HPX_CONFIG_HWLOC], [
 contrib=$1
 pkg=$2
 
 # allow the user to override if we use, and how we find, hwloc
 AC_ARG_ENABLE([hwloc],
   [AS_HELP_STRING([--enable-hwloc],
                   [Enable hwloc topology @<:@default=no@:>@])],
   [], [enable_hwloc=no])

 AC_ARG_WITH(hwloc,
   [AS_HELP_STRING([--with-hwloc{=contrib,system,PKG}],
                   [How we find hwloc @<:@default=contrib@:>@])],
   [], [with_hwloc=contrib])
   
 # Handle the --with-hwloc option if hwloc is enabled, otherwise we ignore it.
 AS_IF([test "x$enable_hwloc" != xno], [_HPX_WITH_HWLOC($pkg)])

 # If we want the contributed HWLOC then configure and build it. We do this
 # with the test (rather than in the case above) because we can't have the
 # HWLOC_SETUP_CORE macro appear more than once---even conditionally.
 AS_IF([test "x$build_hwloc" == xyes], [_HPX_CONTRIB_HWLOC($contrib)])
])
