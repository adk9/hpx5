# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBFFI([src-path],[prefix])
#
# Substitutes
#   HPX_LIBFFI_CPPFLAGS
#   HPX_LIBFFI_LDFLAGS
#
# ------------------------------------------------------------------------------

AC_ARG_VAR([HPX_LIBFFI_CARGS], [Additional arguments passed to libffi contrib])

AC_DEFUN([HPX_CONTRIB_LIBFFI],
  [AC_ARG_ENABLE([external-libffi],
    [AS_HELP_STRING([--enable-external-libffi],
                    [Enable the use of system-installed libffi @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([LIBFFI], [libffi], [],
        [AC_MSG_WARN([pkg-config could not find libffi])
         AC_MSG_WARN([falling back to {LIBFFI_CFLAGS, LIBFFI_CPPFLAGS, LIBFFI_LIBS} variables])])
        AC_SUBST(HPX_LIBFFI_CPPFLAGS, "$LIBFFI_CPPFLAGS")
        AC_SUBST(HPX_LIBFFI_CFLAGS, "$LIBFFI_CFLAGS")
        AC_SUBST(HPX_LIBFFI_LIBS, "$LIBFFI_LIBS")
	enable_external_libffi=yes],
      [ACX_CONFIGURE_DIR([$1], [$1], [" $HPX_LIBFFI_CARGS"])
        AC_SUBST(HPX_LIBFFI_CPPFLAGS, " -I\$(top_builddir)/$1/include")
        AC_SUBST(HPX_LIBFFI_DEPS, "\$(abs_top_builddir)/$1/libffi.la")
        AC_SUBST(HPX_LIBFFI_LIBS, "\$(abs_top_builddir)/$1/libffi.la")
	# the ffi convenience library allows us to always link this contrib statically
	# will need to revisit when we figure out building a shared libhpx
	enable_external_libffi=no])]
  AM_CONDITIONAL([BUILD_LIBFFI], [test "x$enable_external_libffi" == xno]))
