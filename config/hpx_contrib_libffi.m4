# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBFFI
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_LIBFFI],
  [ACX_CONFIGURE_DIR([$1], [$1])
   AC_SUBST(HPX_LIBFFI_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_LIBFFI_LDADD, "\$(top_builddir)/$1/libffi.la")])
