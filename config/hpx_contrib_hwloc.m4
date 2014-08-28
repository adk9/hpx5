# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_HWLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_HWLOC],
  [HWLOC_SET_SYMBOL_PREFIX([$2hwloc_])
   HWLOC_SETUP_CORE([$1], [have_hwloc=yes], [have_hwloc=no])
   AS_IF([test "x$have_hwloc" != xyes],
     [AC_MSG_ERROR([could not configure hwloc])],
     [HWLOC_DO_AM_CONDITIONALS])])
