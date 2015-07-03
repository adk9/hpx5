# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBFFI
#
# libffi is required by HPX for execution. HPX ships with all of its required
# dependencies, however, we prefer a system-installed version of libffi over the
# contribed version. At the same time, we will allow the user to override this
# preference using a standard --with-libffi option.
#
# At the end of the day, in a successful configuration, this will set HPX_DEPS,
# CFLAGS and LIBS as necessary to build the package, and set HPX_REQUIRES_PKGS
# and HPX_PUBLIC_LIBS for installation. It will #define HAVE_LIBFFI on success
# as well.
# ------------------------------------------------------------------------------
AC_DEFUN([_HPX_CONFIGURE_LIBFFI],
[# configure the contributed libffi package
 ACX_CONFIGURE_DIR([$1], [$1], [" "])
 CPPFLAGS="$CPPFLAGS -I\$(top_builddir)/$1/include"
 LIBS="$LIBS \$(top_builddir)/$1/libffi.la"
 HPX_DEPS="$HPX_DEPS \$(top_builddir)/$1/libffi.la"
 HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS libffi"
 build_libffi=yes
 have_libffi=yes
])

AC_DEFUN([_HPX_PACKAGE_LIBFFI],
[# try and find a pkg-config package for libffi
 PKG_CHECK_MODULES([LIBFFI], [$1],
   [CFLAGS="$CFLAGS $LIBFFI_CFLAGS"
    LIBS="$LIBS $LIBFFI_LIBS"
    HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS $1"
    have_libffi=yes],
   [have_libffi=no])
])

AC_DEFUN([_HPX_LIB_LIBFFI],
[# Search the current path for libffi
 AC_CHECK_LIB([ffi], [ffi_raw_size],
   [LIBS="$LIBS -lffi"
    HPX_PUBLIC_LIBS="$HPX_PUBLIC_LIBS -lffi"
    have_libffi=yes])
])

AC_DEFUN([HPX_CONTRIB_LIBFFI],
[# allow the user to override the way we try and find libffi
 AC_ARG_WITH(libffi,
   [AS_HELP_STRING([--with-libffi={auto,system,contrib,PKG}],
                   [How we find libffi @<:@default=auto@:>@])],
   [], [with_libffi=auto])

 AS_CASE($with_libffi,

   # contrib means just go ahead and configure and build the contributed version
   # of libffi
   [contrib], [_HPX_CONFIGURE_LIBFFI($1)],
     
   # system indicates that we should look for a system-installed libffi, first
   # in the current path and then as a default-named pkg-config package
   [system], [_HPX_LIB_LIBFFI()
              AS_IF([test "x$have_libffi" != xyes],
                [_HPX_PACKAGE_LIBFFI(libffi)])],
        
   # auto indicates that we should first look for a libffi in our path, then
   # look for a default-named pkg-config package, and then finally resort to the
   # contributed version of libffi
   [auto], [_HPX_LIB_LIBFFI()
            AS_IF([test "x$have_libffi" != xyes], [_HPX_PACKAGE_LIBFFI(libffi)])
            AS_IF([test "x$have_libffi" != xyes], [_HPX_CONFIGURE_LIBFFI($1)])],
       
   # any other string is interpreted as a custom pkg-config package name to use
   [_HPX_PACKAGE_LIBFFI($with_libffi)])

 # libffi is required
 AS_IF([test "x$have_libffi" != xyes],
   [AC_MSG_ERROR([Failed to find libffi for --with-libffi=$with_libffi])])

 AC_DEFINE([HAVE_LIBFFI], [1], [We have libffi])
 AM_CONDITIONAL([BUILD_LIBFFI], [test "x$build_libffi" == xyes])
])

