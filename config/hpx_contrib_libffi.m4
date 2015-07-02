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

AC_DEFUN([HPX_CONTRIB_LIBFFI],
 [# allow the user to override the way we try and find libffi
  AC_ARG_WITH(libffi,
   [AS_HELP_STRING([--with-libffi={auto,system,contrib,PKG}],
                   [How we find libffi @<:@default=auto@:>@])],
     [], [with_libffi=auto])

  AS_CASE($with_libffi,
   # contrib means just go ahead and build the contrib version
   [contrib],
    [ACX_CONFIGURE_DIR([$1], [$1], [" "])
     CFLAGS="$CFLAGS -I\$(top_builddir)/$1/include"
     LIBS="$LIBS \$(top_builddir)/$1/libffi.la"
     HPX_DEPS="$HPX_DEPS \$(top_builddir)/$1/libffi.la"
     HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS libffi"
     build_libffi=yes
     have_libffi=yes],
     
   # system means check if there is a libffi in the path, or a libffi package
   # available 
   [system],
    [AC_CHECK_LIB([ffi], [ffi_raw_size],
      [LIBS="$LIBS -lffi"
       HPX_PUBLIC_LIBS="$HPX_PUBLIC_LIBS -lffi"
       have_libffi=yes])
     AS_IF([test "x$have_libffi" != xyes],
      [PKG_CHECK_MODULES([LIBFFI], [libffi],
       [CFLAGS="CFLAGS $LIBFFI_CFLAGS"
        LIBS="$LIBS $LIBFFI_LIBS"
        HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS libffi"
        have_libffi=yes])])],
        
   # auto means first try the system, then try the contrib
   [auto],
    [AC_CHECK_LIB([ffi], [ffi_raw_size],
      [LIBS="$LIBS -lffi"
       HPX_PUBLIC_LIBS="$HPX_PUBLIC_LIBS -lffi"
       have_libffi=yes])
     AS_IF([test "x$have_libffi" != xyes],
      [PKG_CHECK_MODULES([LIBFFI], [libffi],
       [CFLAGS="$CFLAGS $LIBFFI_CFLAGS"
        LIBS="$LIBS $LIBFFI_LIBS"
        HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS libffi"
        have_libffi=yes],
       [have_libffi=no])])
     AS_IF([test "x$have_libffi" != xyes],
      [ACX_CONFIGURE_DIR([$1], [$1], [" "])
       CFLAGS="$CFLAGS -I\$(top_builddir)/$1/include"
       LIBS="$LIBS \$(top_builddir)/$1/libffi.la"
       HPX_DEPS="$HPX_DEPS \$(top_builddir)/$1/libffi.la"
       HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS libffi"
       build_libffi=yes
       have_libffi=yes])],
       
   # we interpret any other string as a pkg-config package id
   [PKG_CHECK_MODULES([LIBFFI], [$with_libffi],
    [CFLAGS="$CFLAGS $LIBFFI_CFLAGS"
     LIBS="$LIBS $LIBFFI_LIBS"
     HPX_REQUIRES_PKGS="$HPX_REQUIRES_PKGS $with_libffi"
     have_libffi=yes],
    [AC_MSG_WARN([Could not find libffi package $with_libffi])])])

  # make sure we successfully found a libffi
  AS_IF([test "x$have_libffi" != xyes],
   [AC_MSG_ERROR([Failed to find libffi for --with-libffi=$with_libffi])],
   [AC_DEFINE([HAVE_LIBFFI], [1], [The libhpx library version])])

  # if we want to use the contrib libffi then set an AM conditional for it
  AM_CONDITIONAL([BUILD_LIBFFI], [test "x$build_libffi" == xyes])])
