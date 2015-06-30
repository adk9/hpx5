# -*- autoconf -*---------------------------------------------------------------
# PHOTON_CONTRIB_LIBFABRIC([src-path],[prefix])
# ------------------------------------------------------------------------------
# Make sure that a compatible installation of libfabric can be found.
#
# Sets
#   photon_have_libfabric
#
# Substitutes
#   PHOTON_LIBFABRIC_CPPFLAGS
#   PHOTON_LIBFABRIC_LDFLAGS
#
# ------------------------------------------------------------------------------

AC_ARG_VAR([PHOTON_LIBFABRIC_CARGS], [Additional arguments passed to libfabric contrib])

AC_DEFUN([PHOTON_CONTRIB_LIBFABRIC],
  [AC_ARG_ENABLE([external-libfabric],
    [AS_HELP_STRING([--enable-external-libfabric],
                    [Enable the use of system-installed libfabric @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([LIBFABRIC], [libfabric], [],
        [AC_MSG_WARN([pkg-config could not find libfabric])
         AC_MSG_WARN([falling back to {LIBFABRIC_CFLAGS, LIBFABRIC_CPPFLAGS, LIBFABRIC_LIBS} variables])])
        AC_SUBST(PHOTON_LIBFABRIC_CPPFLAGS, "$LIBFABRIC_CPPFLAGS")
        AC_SUBST(PHOTON_LIBFABRIC_CFLAGS, "$LIBFABRIC_CFLAGS")
        AC_SUBST(PHOTON_LIBFABRIC_LIBS, "$LIBFABRIC_LIBS")
	enable_external_libfabric=yes],
      [ACX_CONFIGURE_DIR([$1], [$1], [" $PHOTON_LIBFABRIC_CARGS"])
      	AC_SUBST(PHOTON_LIBFABRIC_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
	AC_SUBST(PHOTON_LIBFABRIC_DEPS, "\$(top_builddir)/$1/src/libfabirc.la")
        AC_SUBST(PHOTON_LIBFABRIC_LIBS, "\$(top_builddir)/$1/src/libfabric.la")
	enable_external_libfabric=no])]
  AC_DEFINE([HAVE_LIBFABRIC], [1], [libfabric support enabled]))
