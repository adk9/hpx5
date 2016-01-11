# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_LIBHPX
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_LIBHPX], [
  # libhpx uses the compiler's unwind.h header to provide stack unwinding for
  # C++ applications. unwind.h is fairly standard, however there are some tiny
  # differences in different platforms w.r.t. some typedefs. In particular, the
  # standard uses uint64_t for the exception_class type, but some arm platforms
  # use char[8]. These platforms use a non-standard typedef of
  # _Unwind_Exception_Class to refer to exception class types.
  #
  # The _Unwind_Exception_Class typedef is fairly standard across platforms, and
  # we'd like to use it directly, however the default unwind.h header for Mac OS
  # X 10.11 doesn't have this typedef, it uses _Unwind_Exception_Class. We use a
  # platform check to typedef this value if its not found.
  AC_CHECK_TYPE([_Unwind_Exception_Class],
                [AC_DEFINE([HAVE_UNWIND_EXCEPTION_CLASS], [1],
                 [unwind.h typedefs _Unwind_Exception_Class])],
                [], [#include <unwind.h>])
  
  # Make sure that static linking libhpx.a with $(CC) works when we have a C++
  # dependency built into libhpx. This is for C applications, external C++
  # clients will have to deal with linking libstdc++ on their own if they need
  # it (they're likely using CC to link).
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
