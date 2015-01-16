# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [ACX_CONFIGURE_DIR([contrib/jemalloc], [contrib/jemalloc_build],
                     ["--disable-valgrind --disable-fill --disable-stats --with-jemalloc-prefix=libhpx_global_ --without-mangling"])
 
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, " -I\$(top_builddir)/contrib/jemalloc_build/include")
   AC_SUBST(HPX_JEMALLOC_LDFLAGS, " -L\$(top_builddir)/contrib/jemalloc_build/lib/")
   AC_SUBST(HPX_JEMALLOC_LIBS, " -ljemalloc")
   AC_SUBST(HPX_JEMALLOC_LDADD, "\$(top_builddir)/contrib/jemalloc_build/lib/libjemalloc.a")])
