# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_VALGRIND([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_VALGRIND], [$2valgrind_cppflags="-I\$(top_srcdir)/$1/include"])
