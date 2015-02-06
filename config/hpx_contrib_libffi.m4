# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBFFI([src-path],[prefix])
#
# Substitutes
#   HPX_LIBFFI_CPPFLAGS
#   HPX_LIBFFI_LDFLAGS
#   HPX_LIBFFI_LDADD
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_LIBFFI],
  [AC_ARG_ENABLE([external-libffi],
    [AS_HELP_STRING([--enable-external-libffi],
                    [Enable the use of system-installed libffi @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([LIBFFI], [libffi], [],
        [AC_MSG_WARN([pkg-config could not find libffi])
         AC_MSG_WARN([falling back to {LIBFFI_CFLAGS, LIBFFI_CPPFLAGS, LIBFFI_LIBS} variables])])
        AC_SUBST(HPX_LIBFFI_CPPFLAGS, "\$(LIBFFI_CPPFLAGS)")
        AC_SUBST(HPX_LIBFFI_CFLAGS, "\$(LIBFFI_CFLAGS)")
        AC_SUBST(HPX_LIBFFI_LIBS, "\$(LIBFFI_LIBS)")
	enable_external_libffi=yes],
      [ACX_CONFIGURE_DIR([$1], [$1])
        AC_SUBST(HPX_LIBFFI_CPPFLAGS, " -I\$(top_builddir)/$1/include")
        AC_SUBST(HPX_LIBFFI_LDADD, "\$(abs_top_builddir)/$1/libffi.la")
	enable_external_libffi=no])]
  AM_CONDITIONAL([BUILD_LIBFFI], [test "x$enable_external_libffi" == xno]))
