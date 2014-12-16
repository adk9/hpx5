# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [$2jemalloc_cppflags="-I\$(top_srcdir)/$1/include"

dnl ATTENTION:
dnl The below case was copied from the jemalloc build system, but is used
dnl here in a stripped down version.
dnl This should be kept in sync with jemalloc with a reasonable frequency.

dnl Platform-specific settings.  abi and RPATH can probably be determined
dnl programmatically, but doing so is error-prone, which makes it generally
dnl not worth the trouble.
dnl 
dnl Define cpp macros in CPPFLAGS, rather than doing AC_DEFINE(macro), since the
dnl definitions need to be seen before any headers are included, which is a pain
dnl to make happen otherwise.

case "${host}" in
  *-*-darwin* | *-*-ios*)
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	;;
  *-*-freebsd*)
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	;;
  *-*-dragonfly*)
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	;;
  *-*-linux*)
	JEMALLOC_CPPFLAGS="-D_GNU_SOURCE"
	AC_DEFINE([JEMALLOC_HAS_ALLOCA_H], [ ], [From jemalloc])
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_DONTNEED], [ ], [From jemalloc])
	AC_DEFINE([JEMALLOC_THREADED_INIT], [ ], [From jemalloc])
	;;
  *-*-netbsd*)
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ], [From jemalloc])
	;;
  *-*-solaris2*)
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ], [From jemalloc])
	JEMALLOC_CPPFLAGS="-D_POSIX_PTHREAD_SEMANTICS"
	JEMALLOC_LIBS="-lposix4 -lsocket -lnsl"
	;;
  *-ibm-aix*)
	if "$LG_SIZEOF_PTR" = "8"; then
	  dnl 64bit AIX
	  LD_PRELOAD_VAR="LDR_PRELOAD64"
	else
	  dnl 32bit AIX
	  LD_PRELOAD_VAR="LDR_PRELOAD"
	fi
	;;
  *-*-mingw*)
	if test "x$je_cv_msvc" = "xyes" ; then
	  EXTRA_LDFLAGS="-link -DEBUG"
	fi
	;;
  *)
	AC_MSG_RESULT([JEMALLOC: Unsupported operating system: ${host}])
	;;
esac

   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, " -I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_JEMALLOC_LDADD, "\$(top_builddir)/$1/src/libhpx_jemalloc.la")
   AC_SUBST(HPX_JEMALLOC_BUILD_CPPFLAGS, "$JEMALLOC_CPPFLAGS")
   AC_SUBST(HPX_JEMALLOC_BUILD_LIBS, "$JEMALLOC_LIBS")

   AC_CONFIG_FILES([$1/Makefile])
   AC_CONFIG_FILES([$1/src/Makefile])
   AC_CONFIG_FILES([$1/include/Makefile])
   AC_CONFIG_FILES([$1/include/jemalloc/internal/Makefile])
   AC_CONFIG_FILES([$1/include/jemalloc/Makefile])])
