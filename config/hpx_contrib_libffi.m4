# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBFFI
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_LIBFFI],
  [ACX_CONFIGURE_DIR([libffi-3.2.1], [$1])
   AC_SUBST(HPX_LIBFFI_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_LIBFFI_LDADD, "\$(top_srcdir)/$1/libffi.la")])
