# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix],[suffix])
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_JEMALLOC_CARGS], [Additional arguments passed to jemalloc contrib])

AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [AC_ARG_ENABLE([external-jemalloc],
    [AS_HELP_STRING([--enable-external-jemalloc],
                    [Enable the use of system-installed jemalloc @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([JEMALLOC], [jemalloc], [],
        [AC_MSG_WARN([pkg-config could not find jemalloc])
         AC_MSG_WARN([falling back to {JEMALLOC_CFLAGS, JEMALLOC_CPPFLAGS, JEMALLOC_LIBS} variables])])
        AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "$JEMALLOC_CPPFLAGS")
        AC_SUBST(HPX_JEMALLOC_CFLAGS, "$JEMALLOC_CFLAGS")
        AC_SUBST(HPX_JEMALLOC_LIBS, "$JEMALLOC_LIBS")
	enable_external_jemalloc=yes],
      [ACX_CONFIGURE_DIR([$1], [$1$3],
        ["--disable-shared --disable-valgrind --disable-fill --disable-stats --with-jemalloc-prefix=$2 --without-mangling --with-install-suffix=$3 --with-private-namespace=$2 $HPX_JEMALLOC_CARGS"])
        AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "-I\$(abs_top_builddir)/$1$3/include")
        AC_SUBST(HPX_JEMALLOC_LIBS, "-ljemalloc$3 -L\$(abs_top_builddir)/$1$3/lib")
	AC_SUBST(HPX_JEMALLOC_RPATH, "-R \$(abs_top_builddir)/$1$3/lib")
	enable_external_jemalloc=no])]
  AM_CONDITIONAL([BUILD_JEMALLOC], [test "x$enable_external_jemalloc" == xno]))

AC_ARG_VAR([JEMALLOC_CARGS], [Additional arguments passed to jemalloc contrib])

AC_DEFUN([HPX_CONTRIB_JEMALLOC_AS_MALLOC],
  [AS_IF([test "x$enable_static" != xno],
    [JEMALLOC_CARGS="$JEMALLOC_CARGS --enable-static"],
    [JEMALLOC_CARGS="$JEMALLOC_CARGS --disable-static"])
  AS_IF([test "x$enable_shared" != xno],
    [JEMALLOC_CARGS="$JEMALLOC_CARGS --enable-shared"],
    [JEMALLOC_CARGS="$JEMALLOC_CARGS --disable-shared"])
  ACX_CONFIGURE_DIR([contrib/jemalloc], [contrib/jemalloc],
    ["--disable-valgrind --disable-fill --disable-stats $JEMALLOC_CARGS"])
  AC_SUBST(HPX_JEMALLOC_AS_MALLOC_LIBS, "-ljemalloc -L\$(abs_top_builddir)/contrib/jemalloc/lib")
  AC_SUBST(HPX_JEMALLOC_AS_MALLOC_RPATH, "-R \$(abs_top_builddir)/contrib/jemalloc/lib")
])

