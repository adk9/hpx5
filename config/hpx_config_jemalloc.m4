# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_JEMALLOC([path], [pkg])
#
# Sets
#   enable_jemalloc
#   with_jemalloc
#   have_jemalloc
#   build_jemalloc
#
# Appends
#   LIBHPX_CPPFLAGS
#   LIBHPX_LIBADD
#   LIBHPX_LIBS
#   HPX_PC_PRIVATE_PKGS
#   HPX_PC_PRIVATE_LIBS
#
# Defines
#   HAVE_JEMALLOC
#   HAVE_AS_GLOBAL
#   HAVE_AS_REGISTERED
# ------------------------------------------------------------------------------
AC_DEFUN([_HAVE_JEMALLOC], [
 AC_DEFINE([HAVE_JEMALLOC], [1], [We have the jemalloc allocator])
 have_jemalloc=yes
])

AC_DEFUN([_HPX_CONTRIB_JEMALLOC], [
 contrib=$1
 
 # set up the configuration options
 jemalloc_options="--disable-valgrind --disable-fill --disable-stats"
 jemalloc_c99_flags="EXTRA_CFLAGS=$ac_cv_prog_cc_c99"
 jemalloc_debug_flags="CPPFLAGS=\"$CPPFLAGS -Dalways_inline=\""
 jemalloc_cargs="$jemalloc_options $jemalloc_c99_flags"
 AS_IF([test "x$enable_debug" == xyes],
   [jemalloc_cargs="$jemalloc_cargs $jemalloc_debug_flags"])

 # perform the configuration, installing the docs so that 'make install' works 
 ACX_CONFIGURE_DIR([$contrib], [$contrib], ["$jemalloc_cargs"])
 AC_CONFIG_FILES([$contrib/doc/jemalloc.html:$contrib/doc/jemalloc.html
                  $contrib/doc/jemalloc.3:$contrib/doc/jemalloc.3])
 _HAVE_JEMALLOC
 
 # append to hpx variables
 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_builddir)/$contrib/include"

 # We can't build a .la for jemalloc so we need explicit paths and such or
 # internal executables can't link it properly, or find it during execution. The
 # paths have to be abs paths because executables exists at many different
 # directory depths (examples, tests/unit, etc), so no relative .. works for
 # every case.
 LIBHPX_LDFLAGS="-L\$(abs_top_builddir)/$contrib/lib $LIBHPX_LDFLAGS"
 LIBHPX_LDFLAGS="-Wl,-rpath,\$(abs_top_builddir)/$contrib/lib $LIBHPX_LDFLAGS"
 LIBHPX_LIBS="-ljemalloc $LIBHPX_LIBS" 

 # we install the jemalloc pkg-config script with hpx.pc so external
 # applications will get jemalloc as a private package
 HPX_PC_PRIVATE_PKGS="jemalloc $HPX_PC_PRIVATE_PKGS"
])

# search for a jemalloc pkg-config package
AC_DEFUN([_HPX_PKG_JEMALLOC], [
 pkg=$1
 
 PKG_CHECK_MODULES([JEMALLOC], [$pkg],
   [_HAVE_JEMALLOC
    LIBHPX_CFLAGS="$LIBHPX_CFLAGS $JEMALLOC_CFLAGS"
    LIBHPX_LIBS="$JEMALLOC_LIBS $LIBHPX_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkg"])
])

# look for jemalloc in the path
AC_DEFUN([_HPX_CC_JEMALLOC], [
 AC_CHECK_HEADER([jemalloc.h],
   [AC_CHECK_LIB([jemalloc], [jemalloc_init],
     [_HAVE_JEMALLOC
      LIBHPX_LIBS="-ljemalloc $LIBHPX_LIBS"
      HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -ljemalloc"])])
])

AC_DEFUN([_HPX_WITH_JEMALLOC], [
 pkg=$1

 # handle the with_jemalloc option, if enable_jemalloc is selected
 AS_CASE($with_jemalloc,
   [no], [AC_MSG_ERROR([--enable-jemalloc=$enable_jemalloc excludes --without-jemalloc])],
   
   # contrib means we should just go ahead and build the library, which is also
   # the default if just --with-jemalloc is specified
   [contrib], [build_jemalloc=yes],
   [yes], [build_jemalloc=yes],
   
   # system means that we look for a library in the system path, or a
   # default-named pkg-config package
   [system], [_HPX_CC_JEMALLOC
              AS_IF([test "x$with_jemalloc" != xyes], [_HPX_PKG_JEMALLOC($pkg)])],

   # any other string is interpreted as a custom pkg-config package
   [_HPX_PKG_JEMALLOC($with_jemalloc)])
])

AC_DEFUN([HPX_CONFIG_JEMALLOC], [
 contrib=$1
 pkg=$2
 
 # Allow the user to override the way we try and find jemalloc.
 AC_ARG_ENABLE([jemalloc],
   [AS_HELP_STRING([--enable-jemalloc],
                   [Enable the jemalloc allocator @<:@default=yes@:>@])],
   [], [enable_jemalloc=yes])

 AC_ARG_WITH(jemalloc,
   [AS_HELP_STRING([--with-jemalloc{=contrib,system,PKG}],
                   [How we find jemalloc @<:@default=contrib@:>@])],
   [], [with_jemalloc=contrib])

 AS_IF([test "x$enable_jemalloc" != xno], [_HPX_WITH_JEMALLOC($pkg)])

 # unified call to _HPX_CONTRIB_JEMALLOC because it tries to expand
 # HPX_CONFIG_FILES which must only be expanded once---call before
 # AM_CONDITIONAL because it sets have_jemalloc on success
 AS_IF([test "x$build_jemalloc" == xyes], [_HPX_CONTRIB_JEMALLOC($contrib)])
])
