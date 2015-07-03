# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_VALGRIND([path])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_VALGRIND],
 [CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/$1/include"])
