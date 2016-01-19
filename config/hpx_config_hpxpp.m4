# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_HPXPP
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_HPXPP], [
 AC_ARG_ENABLE([hpxpp],
   [AS_HELP_STRING([--enable-hpxpp], [Enable HPX++ bindings (requires C++11) @<:@default=no@:>@])],
   [], [enable_hpxpp=no])

 AS_IF([test "x$enable_hpxpp" != xno],
   [AC_DEFINE([HAVE_HPXPP], [1], [We have hpx++])
    AX_CXX_COMPILE_STDCXX_11([ext],[mandatory])
    dnl HPX_APPS_CPPFLAGS="$HPX_APPS_CPPFLAGS -I\$(top_srcdir)/include/hpx++"
    have_hpxpp=yes])
])
