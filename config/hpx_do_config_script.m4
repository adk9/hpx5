# -*- autoconf -*---------------------------------------------------------------
# HPX_DO_CONFIG_SCRIPT
#
# Set automake conditionals for use in hpx-config.
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_DO_CONFIG_SCRIPT], [
  CONFIG=`echo ${ac_configure_args} | sed -e 's#'"'"'\([^ ]*\)'"'"'#\1#g'`
  AC_SUBST([CONFIG])
])
