# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_LIBFFI
#
# libffi is required by HPX for execution. HPX ships with all of its required
# dependencies, however, we prefer a system-installed version of libffi over the
# contribed version. At the same time, we will allow the user to override this
# preference using a standard --with-libffi option.
#
# Variables
#   with_libffi
#   have_libffi
#   build_libffi
#
# Appends
#   LIBHPX_CPPFLAGS
#   LIBHPX_LIBADD
#   LIBHPX_LIBS
#   HPX_PC_REQUIRES_PKGS
#   HPX_PC_PUBLIC_LIBS
#
# Defines
#   HAVE_LIBFFI
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_LIBFFI], [
 AC_DEFINE([HAVE_LIBFFI], [1], [We have libffi])
 have_libffi=yes
])

AC_DEFUN([_HPX_CONTRIB_LIBFFI], [
 contrib=$1
 
 # Configure the contributed libffi package. We install the pkg-config .pc file
 # for libffi and expose it as a public dependency of HPX, because libffi
 # symbols will appear directly in the application bindary and must be linked
 # directly to libffi, not transitively through libhpx.
 ACX_CONFIGURE_DIR([$contrib], [$contrib], [" "])
 _HAVE_LIBFFI
 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_builddir)/$contrib/include"
 LIBHPX_LIBADD="$LIBHPX_LIBADD \$(top_builddir)/$contrib/libffi.la"
 HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS libffi"
])

AC_DEFUN([_HPX_PKG_LIBFFI], [
 pkg=$1
 
 # try and find a pkg-config package for libffi, the libffi package is *public*
 # in the installed package because ffi symbols will appear in application
 # binaries and must be linked directly to libffi, not simply transitively
 # through libhpx.
 PKG_CHECK_MODULES([LIBFFI], [$pkg],
   [_HAVE_LIBFFI
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $LIBFFI_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $LIBFFI_LIBS"
    HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS $pkg"])
])

AC_DEFUN([_HPX_LIB_LIBFFI], [
 # Search the current path for libffi, and make sure and expose libffi as a
 # public library dependency. This is necessary because application binaries
 # will contain libffi symbols and must be linked directly to libffi, rather
 # than transitively through libhpx.
 AC_CHECK_HEADER([ffi.h], [
   AC_CHECK_LIB([ffi], [ffi_raw_size], [
     _HAVE_LIBFFI
     LIBHPX_LIBS="$LIBHPX_LIBS -lffi"
     HPX_PC_PUBLIC_LIBS="$HPX_PC_PUBLIC_LIBS -lffi"
   ])
 ])
])

AC_DEFUN([HPX_CONFIG_LIBFFI], [
 contrib=$1
 pkg=$2
 
 # allow the user to override the way we try and find libffi.
 AC_ARG_WITH(libffi,
   [AS_HELP_STRING([--with-libffi{=system,contrib,PKG}],
                   [Control which libffi we use @<:@default=system@:>@])],
   [], [with_libffi=yes])

 AS_IF([test "x$with_libffi" == xno],
   [AC_MSG_WARN([libffi is required, defaulting --with-libffi=yes])
    with_libffi=yes])

 AS_CASE($with_libffi,
   [contrib], [build_libffi=yes],
     
   # system indicates that we should look for a system-installed libffi, first
   # in the current path and then as a default-named pkg-config package
   [system], [_HPX_LIB_LIBFFI
              AS_IF([test "x$have_libffi" != xyes], [_HPX_PKG_LIBFFI($pkg)])],
        
   # yes indicates that we should first look for a libffi in our path, then
   # look for a default-named pkg-config package, and then finally resort to the 
   # contributed version of libffi
   [yes], [_HPX_LIB_LIBFFI
            AS_IF([test "x$have_libffi" != xyes], [_HPX_PKG_LIBFFI($pkg)])
            AS_IF([test "x$have_libffi" != xyes], [build_libffi=yes])],
       
   # any other string is interpreted as a custom pkg-config package name to use
   [_HPX_PKG_LIBFFI($with_libffi)])

 AS_IF([test "x$build_libffi" == xyes], [_HPX_CONTRIB_LIBFFI($contrib)])

 AS_IF([test "x$have_libffi" != xyes],
   [AC_MSG_ERROR([Failed to find libffi for --with-libffi=$with_libffi])])
])

