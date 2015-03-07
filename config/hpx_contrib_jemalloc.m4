# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix],[suffix])
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_JEMALLOC_CARGS], [Additional arguments passed to jemalloc contrib])

HPX_JEMALLOC_CARGS="$HPX_JEMALLOC_CARGS --disable-valgrind --disable-fill --disable-stats"

AS_IF([test "x$enable_static" != xno],
  [HPX_JEMALLOC_CARGS="$HPX_JEMALLOC_CARGS --enable-static"],
  [HPX_JEMALLOC_CARGS="$HPX_JEMALLOC_CARGS --disable-static"])

AS_IF([test "x$enable_shared" != xno],
  [HPX_JEMALLOC_CARGS="$HPX_JEMALLOC_CARGS --enable-shared"],
  [HPX_JEMALLOC_CARGS="$HPX_JEMALLOC_CARGS --disable-shared"])

AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [ACX_CONFIGURE_DIR([$1], [$1_build$3],
   ["$HPX_JEMALLOC_CARGS --with-jemalloc-prefix=$2 --with-install-suffix=$3 --with-private-namespace=$2"])
   hpx_jemalloc_cppflags="$hpx_jemalloc_cppflags -I\$(abs_top_builddir)/$1_build$3/include"
   hpx_jemalloc_ldflags="$hpx_jemalloc_ldflags  -L\$(abs_top_builddir)/$1_build$3/lib"
   hpx_jemalloc_libs="$hpx_jemalloc_libs -ljemalloc$3"
   hpx_jemalloc_rpath="$hpx_jemalloc_rpath -R \$(abs_top_builddir)/$1_build$3/lib"
   AC_SUBST(HPX_JEMALLOC_RPATH, "$hpx_jemalloc_cppflags")
   AC_SUBST(HPX_JEMALLOC_LDFLAGS, "$hpx_jemalloc_ldflags")
   AC_SUBST(HPX_JEMALLOC_LIBS, "$hpx_jemalloc_libs")
   AC_SUBST(HPX_JEMALLOC_RPATH, "$hpx_jemalloc_rpath")])
