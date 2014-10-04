# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [$2jemalloc_cppflags="-I\$(top_srcdir)/$1/include"
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "-I\$(top_srcdir)/$1/include")
   AC_SUBST(HPX_JEMALLOC_LDADD, "\$(top_builddir)/$1/src/libhpx_jemalloc.la")

dnl Platform-specific settings.  abi and RPATH can probably be determined
dnl programmatically, but doing so is error-prone, which makes it generally
dnl not worth the trouble.
dnl 
dnl Define cpp macros in CPPFLAGS, rather than doing AC_DEFINE(macro), since the
dnl definitions need to be seen before any headers are included, which is a pain
dnl to make happen otherwise.
default_munmap="1"
case "${host}" in
  *-*-darwin* | *-*-ios*)
	CFLAGS="$CFLAGS"
	abi="macho"
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	RPATH=""
	LD_PRELOAD_VAR="DYLD_INSERT_LIBRARIES"
	so="dylib"
	importlib="${so}"
	force_tls="0"
	DSO_LDFLAGS='-shared -Wl,-dylib_install_name,$(@F)'
	SOREV="${rev}.${so}"
	sbrk_deprecated="1"
	;;
  *-*-freebsd*)
	CFLAGS="$CFLAGS"
	abi="elf"
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	force_lazy_lock="1"
	;;
  *-*-dragonfly*)
	CFLAGS="$CFLAGS"
	abi="elf"
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ])
	;;
  *-*-linux*)
	CFLAGS="$CFLAGS"
	CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
	abi="elf"
	AC_DEFINE([JEMALLOC_HAS_ALLOCA_H], [ ], [From jemalloc])
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_DONTNEED], [ ], [From jemalloc])
	AC_DEFINE([JEMALLOC_THREADED_INIT], [ ], [From jemalloc])
	default_munmap="0"
	;;
  *-*-netbsd*)
	AC_MSG_CHECKING([ABI])
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[#ifdef __ELF__
/* ELF */
#else
#error aout
#endif
]])],
                          [CFLAGS="$CFLAGS"; abi="elf"],
                          [abi="aout"])
	AC_MSG_RESULT([$abi])
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ], [From jemalloc])
	;;
  *-*-solaris2*)
	CFLAGS="$CFLAGS"
	abi="elf"
	AC_DEFINE([JEMALLOC_PURGE_MADVISE_FREE], [ ], [From jemalloc])
	RPATH='-Wl,-R,$(1)'
	dnl Solaris needs this for sigwait().
	CPPFLAGS="$CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
	LIBS="$LIBS -lposix4 -lsocket -lnsl"
	;;
  *-ibm-aix*)
	if "$LG_SIZEOF_PTR" = "8"; then
	  dnl 64bit AIX
	  LD_PRELOAD_VAR="LDR_PRELOAD64"
	else
	  dnl 32bit AIX
	  LD_PRELOAD_VAR="LDR_PRELOAD"
	fi
	abi="xcoff"
	;;
  *-*-mingw*)
	abi="pecoff"
	force_tls="0"
	RPATH=""
	so="dll"
	if test "x$je_cv_msvc" = "xyes" ; then
	  importlib="lib"
	  DSO_LDFLAGS="-LD"
	  EXTRA_LDFLAGS="-link -DEBUG"
	  CTARGET='-Fo$@'
	  LDTARGET='-Fe$@'
	  AR='lib'
	  ARFLAGS='-nologo -out:'
	  AROUT='$@'
	  CC_MM=
        else
	  importlib="${so}"
	  DSO_LDFLAGS="-shared"
	fi
	a="lib"
	libprefix=""
	SOREV="${so}"
	PIC_CFLAGS=""
	;;
  *)
	AC_MSG_RESULT([Unsupported operating system: ${host}])
	abi="elf"
	;;
esac

	AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "$CPPFLAGS -I\$(top_srcdir)/$1/include")

   AC_CONFIG_FILES([contrib/jemalloc/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/src/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/jemalloc/internal/Makefile])
   AC_CONFIG_FILES([contrib/jemalloc/include/jemalloc/Makefile])])
