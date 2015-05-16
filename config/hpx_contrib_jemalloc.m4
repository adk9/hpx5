# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path])
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_JEMALLOC_CARGS], [Additional arguments passed to jemalloc contrib])

AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [hpx_jemalloc_cargs="$HPX_JEMALLOC_CARGS --disable-valgrind --disable-fill --disable-stats"

   AS_IF([test "x$enable_debug" != xno],
     [hpx_jemalloc_cargs="$hpx_jemalloc_cargs CPPFLAGS=\"$CPPFLAGS -Dalways_inline=\""])

   AS_IF([test "x$enable_jemalloc" = xno],
     [hpx_jemalloc_cargs="$hpx_jemalloc_cargs --with-jemalloc-prefix=libhpx_"])
     
   hpx_jemalloc_cargs="$hpx_jemalloc_cargs EXTRA_CFLAGS=$ac_cv_prog_cc_c99"
   
   ACX_CONFIGURE_DIR([$1], [$1], [$hpx_jemalloc_cargs])
   AC_CONFIG_FILES([$1/doc/jemalloc.html:$1/doc/jemalloc.html
                    $1/doc/jemalloc.3:$1/doc/jemalloc.3])
   
   hpx_jemalloc_cppflags="$hpx_jemalloc_cppflags -I\$(abs_top_builddir)/$1/include"
   hpx_jemalloc_ldflags="$hpx_jemalloc_ldflags  -L\$(abs_top_builddir)/$1/lib"
   hpx_jemalloc_libs="$hpx_jemalloc_libs -ljemalloc"
   hpx_jemalloc_rpath="$hpx_jemalloc_rpath -Wl,-rpath,\$(abs_top_builddir)/$1/lib"
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "$hpx_jemalloc_cppflags")
   AC_SUBST(HPX_JEMALLOC_LDFLAGS, "$hpx_jemalloc_ldflags")
   AC_SUBST(HPX_JEMALLOC_LIBS, "$hpx_jemalloc_libs")
   AC_SUBST(HPX_JEMALLOC_RPATH, "$hpx_jemalloc_rpath")])
