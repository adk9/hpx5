AC_PREREQ([2.63])
AC_INIT([photon], [0.1a], [photon@crest.iu.edu])
CLEANFILES="*~ .\#*"
AC_SUBST(CLEANFILES)

AC_CONFIG_SRCDIR([include/config.h.in])
AM_CONFIG_HEADER([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS

# Initialize automake and libtool
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AM_PROG_CC_C_O
AM_PROG_AS

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h inttypes.h malloc.h netdb.h netinet/in.h netinet/in_systm.h stdint.h stdlib.h string.h strings.h sys/param.h sys/queue.h sys/socket.h sys/time.h unistd.h values.h byteswap.h libkern/OSByteOrder.h linux/unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for libraries.
AC_CHECK_LIB([m], [ceilf])
AC_CHECK_LIB([pthread], [pthread_mutex_init], THREAD_LIBS="$THREAD_LIBS -lpthread")
AC_CHECK_LIB([c_r], [pthread_mutex_init], THREAD_LIBS="$THREAD_LIBS -pthread -lc_r")
AC_CHECK_LIB([rt], [sem_init], use_rt=yes)

AM_CONDITIONAL(HAVE_LIBRT, test x$use_rt = xyes)

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_C_CONST
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([atoll atoq gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa memset select socket strcasecmp strchr strdup strerror strlcat strlcpy strstr strtoul strtoll inet_ntop])


# ---------------------------------------------------------------------------
# --enable options
# ---------------------------------------------------------------------------
AC_ARG_ENABLE([testsuite],
  [AS_HELP_STRING([--enable-testsuite],
                  [Enable test suite (requires check) @<:@default=no@:>@])],
  [], [enable_testsuite=no])

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
                  [Enable debug logging @<:@default=no@:>@])],
  [], [enable_debug=no])
AS_IF([test "x$enable_debug" != xno],
  [AC_DEFINE([ENABLE_DEBUG], [1], [Enable debug logging])])

AC_ARG_ENABLE([calltrace],
  [AS_HELP_STRING([--enable-calltrace],
                  [Enable call tracing to file @<:@default=no@:>@])],
  [], [enable_calltrace=no])
AS_IF([test "x$enable_calltrace" != xno],
  [AC_DEFINE([ENABLE_CALLTRACE], [1], [Enable call tracing])])

AC_ARG_ENABLE([verbs],
  [AS_HELP_STRING([--disable-verbs],
		  [disable verbs (IBV) support (default is autodetect)])],
  [enable_verbs=$enableval],
  [enable_verbs=try])

if test "$enable_verbs" != "no" ; then
        AH_TEMPLATE(HAVE_VERBS, Whether or not ibverbs is installed on the system)
        AC_CHECK_LIB(ibverbs, main, [
                AC_CHECK_HEADERS([infiniband/verbs.h], [
                        VERBS_LIBS="-libverbs"
                        have_verbs=yes
                ])
        ])
	AC_CHECK_LIB(rdmacm, main, [
                AC_CHECK_HEADERS([rdma/rdma_cma.h], [
                        VERBS_LIBS+=" -lrdmacm"
                        have_rdmacm=yes
                ])
        ])

        if test "x$have_verbs" == "xyes" && test "x$have_rdmacm" == "xyes"; then
                AC_DEFINE(HAVE_VERBS, 1, Build with VERBS support)
		AC_SUBST(VERBS_LIBS)
		have_vr=yes
        else
		AC_MSG_NOTICE([IB/RDMA Verbs not found.  Try installing OFED or libibverbs-devel and librdmacm-devel])
	fi
fi
AM_CONDITIONAL(HAVE_VERBS, test x$have_vr == xyes)

AC_ARG_ENABLE([libfabric],
 [AS_HELP_STRING([--enable-libfabric], [build with libfabric support @<:@default=no@:>@])], [],
 [enable_libfabric=no])

AS_IF([test "x$enable_libfabric" != xno],
 [PHOTON_CONTRIB_LIBFABRIC([src/contrib/libfabric], [photon_])],
 [photon_have_libfabric=no])
AM_CONDITIONAL(HAVE_LIBFABRIC, test x$photon_have_libfabric == xyes)

PHOTON_WITH_PKG([ugni],[cray-ugni],[Build with UGNI],[no],[UGNI])
PHOTON_WITH_PKG([pmi],[cray-pmi],[Build with PMI],[no],[PMI])
PHOTON_WITH_PKG([mpi],[mpi],[Build with MPI],[no],[MPI])

dnl Check for user defined MPI
AC_ARG_WITH([mpi-include-path], [AC_HELP_STRING([--with-mpi-include-path[=PATH]],
				   [find MPI headers in PATH])], 
				   [with_mpi_include=$withval], 
				   [with_mpi_include=no])

if test "$with_mpi_include" != "no"; then
   AC_MSG_CHECKING(for mpi.h in $with_mpi_include)
   if test -f $with_mpi_include/mpi.h; then
   	  AC_MSG_RESULT(yes)
	  AC_SUBST(MPI_CFLAGS, "-I$with_mpi_include")
	  AC_DEFINE(HAVE_MPI)
	  have_mpi=yes
   else
	  AC_MSG_RESULT(no)
      AC_MSG_ERROR(No MPI headers found!)
   fi
fi

AC_ARG_WITH([mpi-ld-path], [AC_HELP_STRING([--with-mpi-ld-path[=PATH]],
                   [find MPI libraries in PATH])],
                   [with_mpi_lib=$withval],
                   [with_mpi_lib=no])

if test "$with_mpi_lib" != "no"; then
   #AC_MSG_CHECKING(for MPI libs in $with_mpi_lib with $LDFLAGS)
   save_LDFLAGS=$LDFLAGS
   LDFLAGS="-L$with_mpi_lib"
   AC_CHECK_LIB([mpi], [MPI_Init], [AC_SUBST(MPI_LIBS, "-L$with_mpi_lib -lmpi")],
   		       		    [AC_CHECK_LIB([mpich], [MPI_Init],
				       [AC_SUBST(MPI_LIBS, "-L$with_mpi_lib -lmpich")],
				       [AC_MSG_ERROR(No MPI libs found!)],
				       [ -lpthread])],
				   [-lpthread])
   LDFLAGS=$save_LDFLAGS
fi

AM_CONDITIONAL(HAVE_MPI, test x$have_mpi = xyes || test x$with_mpi != xno)

dnl CHeck for user defined XSP
AC_ARG_WITH([xsp-include-path], [AC_HELP_STRING([--with-xsp-include-path[=PATH]],
				   [find XSP headers in PATH])], 
				   [with_xsp_include=$withval], 
				   [with_xsp_include=no])

if test "$with_xsp_include" != "no"; then
   AC_MSG_CHECKING(for libxsp_client.h in $with_xsp_include)
   if test -f $with_xsp_include/libxsp_client.h; then
   	  AC_MSG_RESULT(yes)
	  AC_SUBST(XSP_CFLAGS, "-I$with_xsp_include")
	  AC_DEFINE(HAVE_XSP, 1, "Build with XSP support")
	  have_xsp=yes
   else
	  AC_MSG_RESULT(no)
      AC_MSG_ERROR(No MPI headers found!)
   fi
fi

AC_ARG_WITH([xsp-ld-path], [AC_HELP_STRING([--with-xsp-ld-path[=PATH]],
                   [find XSP libraries in PATH])],
                   [with_xsp_lib=$withval],
                   [with_xsp_lib=no])

dnl -- Need a separate check for SSL instead of assuming it's there
if test "$with_xsp_lib" != "no"; then
   #AC_MSG_CHECKING(for XSP libs in $with_xsp_lib)
   save_LDFLAGS=$LDFLAGS
   LDFLAGS="-L$with_xsp_lib"
   AC_CHECK_LIB([xsp_client], [libxsp_init], [AC_SUBST(XSP_LIBS, "-L$with_xsp_lib -lxsp_client -lssl")],
                [AC_SUBST(XSP_LIBS, "-lxsp_client")], [-lssl])
   LDFLAGS=$save_LDFLAGS
fi
AM_CONDITIONAL(HAVE_XSP, test x$have_xsp = xyes)

# ---------------------------------------------------------------------------
# Perform host-specific work here.
# ---------------------------------------------------------------------------

AS_CASE([$host_os],
     [linux*], [AS_CASE([$host_cpu],
         [arm*], [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
             l1d_linesize=32
             pagesize=`getconf PAGESIZE`],
         [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
             dnl l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
             l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
             pagesize=`getconf PAGESIZE`])],
     [darwin*], [l1d_linesize=`sysctl -n hw.cachelinesize`
                 pagesize=`getconf PAGESIZE`],
    [solaris*], [l1d_linesize=`prtpicl -v -c cpu | \
                               grep l1-dcache-line-size | uniq | \
                               awk '{print $2}'`]
                [AC_MSG_WARN([Unexpected Host OS $host_os, using defaults])
                 l1d_linesize=128
                 pagesize=4096])

AC_DEFINE_UNQUOTED([PHOTON_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
AC_DEFINE_UNQUOTED([PHOTON_PAGE_SIZE], [$pagesize], [OS Memory Page Size])
AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
AC_DEFINE_UNQUOTED([HPXPAGE_SIZE], [$pagesize], [OS Memory Page Size])

# ---------------------------------------------------------------------------
# --with-options, see config/photon_with_pkg.m4 for details
# ---------------------------------------------------------------------------
PHOTON_WITH_PKG([check],[check],[build with check unit tests],[no],[CHECK])
AS_IF([test "x$enable_testsuite" != xno && test "x$with_check" != xno],
  [enable_tests=yes],
  [enable_tests=no])
AM_CONDITIONAL([PHOTON_ENABLE_TESTS], [test "x$enable_tests" != xno])
AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([BUILD_LIBFABRIC], [test "x$enable_external_libfabric" == xno])

# Set source and build directories
AC_SUBST(PHOTON_SRCDIR, "\${top_srcdir}")
AC_SUBST(PHOTON_BUILDDIR, "\${top_builddir}")

AC_CONFIG_FILES([
	Makefile
	src/contrib/Makefile
	src/contrib/bit_array/Makefile
	src/contrib/libsync/Makefile
	src/contrib/libsync/arch/Makefile
	src/util/Makefile
	src/Makefile
	photon.pc
])

AS_IF([test "x$enable_tests" != xno],
  [AC_CONFIG_FILES([tests/Makefile])
   AS_IF([test x$have_mpi = xyes || test x$with_mpi != xno],
     [AC_CONFIG_FILES([test/Makefile])])])

AC_OUTPUT

AC_SUBST(THREAD_LIBS)

cat << EOF
Config files successfully generated by ./configure $ac_configure_args !

  Install prefix     : $prefix
    
  To build and install, run:
     $ make
     $ make install

EOF
