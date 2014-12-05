# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_PHOTON([path],[prefix])
# ------------------------------------------------------------------------------
# Make sure that a compatible installation of photon can be found.
#
# Sets
#   hpx_with_photon_include_dir
#   hpx_with_photon_lib_dir
#   hpx_with_photon_lib
#   hpx_have_photon
#
# Substitutes
#   HPX_PHOTON_CPPFLAGS
#   HPX_PHOTON_LDFLAGS
#   HPX_PHOTON_LIBS
#   HPX_PHOTON_LDADD
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_PHOTON],
 [AC_MSG_CHECKING([if Photon is installed])

  # Make sure we're using C and checkpoint flags that we might overwrite.
  AC_LANG_PUSH(C)
  hpx_old_photon_CPPFLAGS="$CPPFLAGS"
  hpx_old_photon_LDFLAGS="$LDFLAGS"
  hpx_old_photon_LIBS="$LIBS"

  # Allow the user to set explicit paths to photon.
  AC_ARG_WITH([photon-include-dir],
   [AS_HELP_STRING([--with-photon-include-dir], [path to <photon.h>])],
   [hpx_with_photon_include_dir=$withval],
   [hpx_with_photon_include_dir=no]
  )
  
  AS_IF([test "x$hpx_with_photon_include_dir" != xno],
   [AC_SUBST(HPX_PHOTON_CPPFLAGS, ["-I$hpx_with_photon_include_dir"])]
  )
   
  AC_ARG_WITH([photon-lib-dir],
   [AS_HELP_STRING([--with-photon-lib-dir], [path to libphoton.{a,so}])],
    [hpx_with_photon_lib_dir=$withval
     AS_IF([test "x$hpx_with_photon_lib_dir" != xno],
      [AC_SUBST(HPX_PHOTON_LDFLAGS,
        ["-L$hpx_with_photon_lib_dir -Wl,-rpath,$hpx_with_photon_lib_dir"])
      ]
     )

     AC_ARG_WITH([photon-lib],
      [AS_HELP_STRING([--with-photon-lib], [photon library @<:@default=photon@:>@])],
      [hpx_with_photon_lib=$withval],
      [hpx_with_photon_lib=photon]
     )
     AC_SUBST(HPX_PHOTON_LIBS, [-l"$hpx_with_photon_lib"])
   ],
   [hpx_with_photon_lib_dir=no]
  )

  AS_IF([test "x$hpx_with_photon_include_dir" == xno -o "x$hpx_with_photon_lib_dir" == xno],
   [AC_MSG_RESULT([no])
    AC_MSG_CHECKING([whether to build internal Photon])

    AC_CONFIG_SRCDIR([$1/include/config.h.in])
    AM_CONFIG_HEADER([$1/include/config.h])

    # Set source and build directories
    AC_SUBST(PHOTON_SRCDIR, "\$(top_srcdir)/$1")
    AC_SUBST(PHOTON_BUILDDIR, "\$(top_builddir)/$1")

    $2photon_cppflags="-I\$(top_srcdir)/$1/include"
    AC_SUBST(HPX_PHOTON_CPPFLAGS, "-I\$(top_srcdir)/$1/include")
    AC_SUBST(HPX_PHOTON_LDADD, "\$(top_builddir)/$1/src/libphoton.la")
    AC_CONFIG_FILES([$1/photon.pc])
    AC_CONFIG_FILES([$1/Makefile])
    AC_CONFIG_FILES([$1/src/Makefile])
    AC_CONFIG_FILES([$1/src/util/Makefile])
    AC_CONFIG_FILES([$1/src/contrib/Makefile])
    AC_CONFIG_FILES([$1/src/contrib/bit_array/Makefile])
    AC_CONFIG_FILES([$1/src/contrib/libsync/Makefile])
    AC_CONFIG_FILES([$1/src/contrib/libsync/arch/Makefile])
    hpx_have_photon=internal
    AC_MSG_RESULT([yes])],
   [hpx_have_photon=installed
    AC_MSG_RESULT([yes])])

  # TODO: Check if the photon installation we found is usable or not.
  AC_DEFINE([HAVE_PHOTON], [1], [Photon support enabled])

  # restore flags and outer language
  LIBS="$hpx_old_photon_LIBS"
  LDFLAGS="$hpx_old_photon_LDFLAGS"
  CPPFLAGS="$hpx_old_photon_CPPFLAGS"
  AC_LANG_POP
])
