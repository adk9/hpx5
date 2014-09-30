# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [$2jemalloc_cppflags="-I\$(top_srcdir)/$1/include"
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "-I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_JEMALLOC_LDADD, "\$(top_builddir)/$1/src/libhpx_jemalloc.la")

   AC_CONFIG_FILES([contrib/jemalloc/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/src/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/jemalloc/internal/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/jemalloc/Makefile])])
