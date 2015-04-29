# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBCUCKOO([path],[prefix])
# ------------------------------------------------------------------------------
# Sets
#   $prefix_libcuckoo_cppflags
#
# Substitutes
#   LIBCUCKOO_CPPFLAGS
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_LIBCUCKOO],
  [$2libcuckoo_cppflags="-I\$(top_srcdir)/$1"
   AC_SUBST(LIBCUCKOO_CPPFLAGS, [$$2libcuckoo_cppflags])])
