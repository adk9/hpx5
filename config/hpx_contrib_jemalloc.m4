# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix],[suffix])
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_JEMALLOC_CARGS], [Additional arguments passed to jemalloc contrib])

AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [$2jemalloc$3_cargs="$HPX_JEMALLOC_CARGS --disable-valgrind --disable-fill --disable-stats"

   AS_IF([test "x$enable_static" != xno],
     [$2jemalloc$3_cargs="$$2jemalloc$3_cargs --enable-static"],
     [$2jemalloc$3_cargs="$$2jemalloc$3_cargs --disable-static"])

   AS_IF([test "x$enable_shared" != xno],
     [$2jemalloc$3_cargs="$$2jemalloc$3_cargs --enable-shared"],
     [$2jemalloc$3_cargs="$$2jemalloc$3_cargs --disable-shared"])

   AS_IF([test "x$enable_debug" != xno],
     [$2jemalloc$3_cargs="$$2jemalloc$3_cargs CPPFLAGS=\"$CPPFLAGS -Dalways_inline=\""])
   
   $2jemalloc$3_cargs="$$2jemalloc$3_cargs --with-jemalloc-prefix=$2 --with-install-suffix=$3 --with-private-namespace=$2 EXTRA_CFLAGS=$ac_cv_prog_cc_c99"
   
   ACX_CONFIGURE_DIR([$1], [$1_build$3], [$$2jemalloc$3_cargs])
   AC_CONFIG_FILES([$1_build$3/doc/jemalloc$3.html:$1/doc/jemalloc$3.html
                    $1_build$3/doc/jemalloc$3.3:$1/doc/jemalloc$3.3])
   
   hpx_jemalloc_cppflags="$hpx_jemalloc_cppflags -I\$(abs_top_builddir)/$1_build$3/include"
   hpx_jemalloc_ldflags="$hpx_jemalloc_ldflags  -L\$(abs_top_builddir)/$1_build$3/lib"
   hpx_jemalloc_libs="$hpx_jemalloc_libs -ljemalloc$3"
   hpx_jemalloc_rpath="$hpx_jemalloc_rpath -Wl,-rpath,\$(abs_top_builddir)/$1_build$3/lib"
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "$hpx_jemalloc_cppflags")
   AC_SUBST(HPX_JEMALLOC_LDFLAGS, "$hpx_jemalloc_ldflags")
   AC_SUBST(HPX_JEMALLOC_LIBS, "$hpx_jemalloc_libs")
   AC_SUBST(HPX_JEMALLOC_RPATH, "$hpx_jemalloc_rpath")])
