# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [ACX_CONFIGURE_DIR([jemalloc], [contrib/jemalloc],
                     ["--disable-valgrind --disable-fill --disable-stats --with-jemalloc-prefix=libhpx_global_ --without-mangling"])
 
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_JEMALLOC_LIBS, "\$(top_builddir)/$1/lib/libjemalloc.a")])
