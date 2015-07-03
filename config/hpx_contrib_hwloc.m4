# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_HWLOC([path])
# ------------------------------------------------------------------------------
AC_DEFUN([_HPX_CONFIGURE_HWLOC],
[# Disable features that are not required for our embedded build
 enable_libxml2=no
 enable_libnuma=no
 enable_pci=no
 enable_opencl=no
 enable_cuda=no
 enable_nvml=no
 enable_gl=no
 HWLOC_SETUP_CORE([$1],
   [have_hwloc=yes],
   [AC_MSG_WARN([could not configure hwloc])],
   [1])
 unset enable_libxml2
 unset enable_libnuma
 unset enable_pci
 unset enable_opencl
 unset enable_cuda
 unset enable_nvml
 unset enable_gl

 CPPFLAGS="$CPPFLAGS $HWLOC_EMBEDDED_CPPFLAGS"
 HPX_LIBADD="$HPX_LIBADD $HWLOC_EMBEDDED_LDADD"
 HPX_DEPS="$HPX_DEPS $HWLOC_EMBEDDED_LIBS"
])

AC_DEFUN([_HPX_PACKAGE_HWLOC],
[# use pkg-config to find an hwloc module
 PKG_CHECK_MODULES([HWLOC], [$1],
   [CFLAGS="$CFLAGS $HWLOC_CFLAGS"
    LIBS="$LIBS $HWLOC_LIBS"
    have_hwloc=yes],
   [have_hwloc=no])
])

AC_DEFUN([_HPX_LIB_HWLOC],
[# check to see if there is a system-installed version of hwloc
 AC_CHECK_LIB([hwloc], [hwloc_topology_init],
   [LIBS="$LIBS -lhwloc"
    have_hwloc=yes])
])

AC_DEFUN([HPX_CONTRIB_HWLOC],
[# allow the user to override the way we try and find hwloc, and if we use it
 # at all
 AC_ARG_WITH(hwloc,
   [AS_HELP_STRING([--with-hwloc={no,yes,system,contrib,PKG}],
                   [How we find hwloc @<:@default=no@:>@])],
   [], [with_hwloc=yes])

 AS_CASE($with_hwloc,

   # none means that we should build without support for hwloc
   [no], [have_hwloc=no],

   # contrib means that we should just go ahead and configure and use the
   # contributed hwloc library
   [contrib], [build_hwloc=yes],

   # system indicates that we should look for a system-installed hwloc, first in
   # the current path and then a default-named pkg-config package.
   [system], [_HPX_LIB_HWLOC()
              AS_IF([test "x$have_hwloc" != xyes],
                [_HPX_PACKAGE_HWLOC(hwloc)])],

   # Yes indicates that we should search for a useful hwloc in the system path,
   # then as a default-named pkg-config package, and then configure the
   # contributed hwloc if it hasn't been detected in the system.
   [yes], [AC_MSG_NOTICE(automatically detecting hwloc)
           _HPX_LIB_HWLOC()
           AS_IF([test "x$have_hwloc" != xyes], [_HPX_PACKAGE_HWLOC(hwloc)])
           AS_IF([test "x$have_hwloc" != xyes], [build_hwloc=yes])],

   # any other string is interpreted as a custom package name for pkg-config 
   [_HPX_PACKAGE_HWLOC($with_hwloc)])

 # If we want the contributed HWLOC then configure and build it. The
 # HWLOC_DO_AM_CONDITIONALS is meant to be called unconditionally.
 AS_IF([test "x$build_hwloc" == xyes], [_HPX_CONFIGURE_HWLOC($1)])
 HWLOC_DO_AM_CONDITIONALS
  
 AS_IF([test "x$have_hwloc" == xyes],
   [AC_DEFINE([HAVE_HWLOC], [1], [We have hwloc support])])

 AM_CONDITIONAL([BUILD_HWLOC], [test "x$build_hwloc" == xyes])
])
