# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_TUTORIAL
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_TUTORIAL], [
 AC_ARG_ENABLE([tutorial],
  [AS_HELP_STRING([--enable-tutorial],
                  [Enable building of code samples from the HPX tutorial @<:@default=no@:>@])],
  [], [enable_tutorial=no])
])
