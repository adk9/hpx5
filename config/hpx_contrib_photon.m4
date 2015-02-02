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
AC_DEFUN([HPX_CONTRIB_PHOTON],
  [ACX_CONFIGURE_DIR([$1], [$1])
   AC_SUBST(HPX_PHOTON_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_PHOTON_LDADD, "\$(top_builddir)/$1/src/libphoton.la")
   AC_SUBST(HPX_PHOTON_LIBS, "-lphoton")

  # TODO: Check if the photon installation we found is usable or not.
  AC_DEFINE([HAVE_PHOTON], [1], [Photon support enabled])])
