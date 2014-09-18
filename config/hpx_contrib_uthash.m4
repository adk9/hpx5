# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_UTHASH([path],[prefix])
# ------------------------------------------------------------------------------
# Sets
#   $prefix_uthash_cppflags
#
# Substitutes
#   UTHASH_CPPFLAGS
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_UTHASH],
  [$2uthash_cppflags="-I\$(top_srcdir)/$1/src"
   AC_SUBST(UTHASH_CPPFLAGS, "-I\$(top_srcdir)/$1/src")])
