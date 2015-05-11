# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_HWLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_HWLOC],
  [AC_ARG_ENABLE([external-hwloc],
    [AS_HELP_STRING([--enable-external-hwloc],
                    [Enable the use of system-installed HWLOC @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([HWLOC], [hwloc], [],
        [AC_MSG_WARN([pkg-config could not find hwloc])
         AC_MSG_WARN([falling back to {HWLOC_CFLAGS, HWLOC_CPPFLAGS, HWLOC_LIBS} variables])])
        HPX_HWLOC_CPPFLAGS="$HWLOC_CPPFLAGS"
        HPX_HWLOC_CFLAGS="$HWLOC_CFLAGS"
        HPX_HWLOC_LIBS="$HWLOC_LIBS"
        enable_external_hwloc=yes], [])]
   [AC_ARG_ENABLE([hwloc], [AS_HELP_STRING([--enable-hwloc], [Enable HWLOC Support]) @<:@default=no@:>@], [], [enable_hwloc="no"])]
   [AS_IF([test "$enable_hwloc" = "yes"],
      [HWLOC_SET_SYMBOL_PREFIX([$2])
        # Disable features that are not required for our embedded build
        enable_libxml2=no
        enable_libnuma=no
        enable_pci=no
        enable_opencl=no
        enable_cuda=no
        enable_nvml=no
        enable_gl=no
        HWLOC_SETUP_CORE([$1], [have_hwloc=yes], [have_hwloc=no])
        unset enable_libxml2
        unset enable_libnuma
        unset enable_pci
        unset enable_opencl
        unset enable_cuda
        unset enable_nvml
        unset enable_gl
        AS_IF([test "x$have_hwloc" != xyes],
              [AC_MSG_ERROR([could not configure hwloc])],
              [AC_SUBST(HPX_HWLOC_CPPFLAGS, "$HWLOC_EMBEDDED_CPPFLAGS")
               AC_SUBST(HPX_HWLOC_CFLAGS, "$HWLOC_EMBEDDED_CFLAGS")
               AC_SUBST(HPX_HWLOC_LIBS, "$HWLOC_EMBEDDED_LDADD")
               AC_SUBST(HPX_HWLOC_DEPS, "$HWLOC_EMBEDDED_LDADD")])
        enable_external_hwloc=no], [])]
        HWLOC_DO_AM_CONDITIONALS
        AM_CONDITIONAL([BUILD_HWLOC], [test "x$enable_hwloc" == xyes])
        AS_IF([test "x$enable_hwloc" == xyes], [AC_DEFINE([HAVE_HWLOC], [1], [We have HWLOC])], []))
