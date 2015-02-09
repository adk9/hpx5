# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix],[suffix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [AC_ARG_ENABLE([external-jemalloc],
    [AS_HELP_STRING([--enable-external-jemalloc],
                    [Enable the use of system-installed jemalloc @<:@default=no@:>@])],
      [PKG_CHECK_MODULES([JEMALLOC], [jemalloc], [],
        [AC_MSG_WARN([pkg-config could not find jemalloc])
         AC_MSG_WARN([falling back to {JEMALLOC_CFLAGS, JEMALLOC_CPPFLAGS, JEMALLOC_LIBS} variables])])
        AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "\$(JEMALLOC_CPPFLAGS)")
        AC_SUBST(HPX_JEMALLOC_CFLAGS, "\$(JEMALLOC_CFLAGS)")
        AC_SUBST(HPX_JEMALLOC_LIBS, "\$(JEMALLOC_LIBS)")
	enable_external_jemalloc=yes],
      [ACX_CONFIGURE_DIR([$1], [$1$3],
        ["--disable-valgrind --disable-fill --disable-stats --with-jemalloc-prefix=$2 --without-mangling --with-install-suffix=$3 --with-private-namespace=$2"])
        AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "-I\$(top_builddir)/$1$3/include")
        AC_SUBST(HPX_JEMALLOC_LDFLAGS, "-L\$(top_builddir)/$1$3/lib/")
        AC_SUBST(HPX_JEMALLOC_LDADD, "\$(top_builddir)/$1$3/lib/libjemalloc$3.a")
	enable_external_jemalloc=no])]
  AM_CONDITIONAL([BUILD_JEMALLOC], [test "x$enable_external_jemalloc" == xno]))

AC_DEFUN([HPX_CONTRIB_JEMALLOC_AS_MALLOC],
  [ACX_CONFIGURE_DIR([contrib/jemalloc], [contrib/jemalloc],
    ["--disable-valgrind --disable-fill --disable-stats"])
  HPX_JEMALLOC_AS_MALLOC_LDADD="\$(top_builddir)/contrib/jemalloc/lib/libjemalloc.a"])

