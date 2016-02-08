# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_AGAS
#
# Variables
#  have_agas
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_AGAS], [
 AC_ARG_ENABLE([agas],
   [AS_HELP_STRING([--enable-agas], [Enable AGAS (requires C++11) @<:@default=no@:>@])],
   [], [enable_agas=no])

 AS_IF([test "x$enable_agas" != xno],
   [AC_DEFINE([HAVE_AGAS], [1], [We have agas])
    have_agas=yes])

 AC_ARG_ENABLE([agas-rebalancing],
   [AS_HELP_STRING([--enable-agas-rebalancing], [Enable AGAS-based automatic load balancing @<:@default=no@:>@])],
   [], [enable_agas_rebalancing=no])

 AS_IF([test "x$enable_agas_rebalancing" != xno],
   [AC_DEFINE([HAVE_AGAS_REBALANCING], [1], [We have AGAS-based rebalancing])
    have_agas_rebalancing=yes])
])
