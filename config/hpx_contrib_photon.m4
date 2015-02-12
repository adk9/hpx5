# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_PHOTON([src-path],[prefix])
# ------------------------------------------------------------------------------
# Make sure that a compatible installation of photon can be found.
#
# Sets
#   hpx_have_photon
#
# Substitutes
#   HPX_PHOTON_CPPFLAGS
#   HPX_PHOTON_LDFLAGS
#   HPX_PHOTON_LDADD
#
# ------------------------------------------------------------------------------

AC_ARG_VAR([HPX_PHOTON_CARGS], [Additional arguments passed to Photon contrib])

AC_DEFUN([HPX_CONTRIB_PHOTON],
  [AC_ARG_ENABLE([external-photon],
    [AS_HELP_STRING([--enable-external-photon],
                    [Enable the use of system-installed Photon @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([PHOTON], [photon], [],
        [AC_MSG_WARN([pkg-config could not find Photon])
         AC_MSG_WARN([falling back to {PHOTON_CFLAGS, PHOTON_CPPFLAGS, PHOTON_LIBS} variables])])
        AC_SUBST(HPX_PHOTON_CPPFLAGS, "$PHOTON_CPPFLAGS")
        AC_SUBST(HPX_PHOTON_CFLAGS, "$PHOTON_CFLAGS")
        AC_SUBST(HPX_PHOTON_LDADD, "$PHOTON_LIBS")
        AC_SUBST(HPX_PHOTON_LIBS, "$PHOTON_LIBS")
	enable_external_photon=yes],
      [ACX_CONFIGURE_DIR([$1], [$1], [" $HPX_PHOTON_CARGS"])
      	AC_SUBST(HPX_PHOTON_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
        AC_SUBST(HPX_PHOTON_LDADD, "\$(top_builddir)/$1/src/libphoton.la")
	AC_SUBST(HPX_PHOTON_DEPS, "\$(top_builddir)/$1/src/libphoton.la")
        AC_SUBST(HPX_PHOTON_LIBS, "-lphoton $VERBS_LIBS $UGNI_LIBS")
	enable_external_photon=no])]
  AC_DEFINE([HAVE_PHOTON], [1], [Photon support enabled]))
