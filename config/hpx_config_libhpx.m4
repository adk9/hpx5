# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_LIBHPX
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_LIBHPX], [
  AC_CHECK_LIB([m], [log,ceil])
  HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lm"
  LIBHPX_LIBS="$LIBHPX_LIBS -lm"
  HPX_APPS_LIBS="$HPX_APPS_LIBS -lm"

  # Make sure that static linking libhpx.a with $(CC) works when we have a C++
  # dependency.
  AS_IF([test "x$have_tbbmalloc" == xyes -o "x$have_libcuckoo" == xyes],
    [HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lstdc++"
     LIBHPX_LIBS="$LIBHPX_LIBS -lstdc++"
     HPX_APPS_LIBS="$HPX_APPS_LIBS -lstdc++"])

  # Special handling for libsync. Maybe we should make a libsync.pc?
  HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lsync"
  
  # Substitute the variables required for pkg-config linking of libhpx
  # externally. Note if we're only building libhpx.a then the --libs output must
  # contain all private dependencies as well.
  AS_IF([test "x$enable_shared" != xyes],
    [HPX_PC_PUBLIC_LIBS="$HPX_PC_PUBLIC_LIBS $HPX_PC_PRIVATE_LIBS"
     HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS $HPX_PC_PRIVATE_PKGS"])

  # do substitution for the test and example makefiles
  HPX_APPS_LDADD="\$(top_builddir)/libhpx/libhpx.la"
  HPX_APPS_DEPS="\$(top_builddir)/libsync/libsync.la \$(top_builddir)/libhpx/libhpx.la"
])
