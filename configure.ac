AC_PREREQ([2.63])
AC_INIT([photon], [0.1a], [photon@stout.incntre.iu.edu])
CLEANFILES="*~ .\#*"
AC_SUBST(CLEANFILES)

AM_CONFIG_HEADER(include/config.h)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS

# Initialize automake and libtool
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_PROG_CC
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
                  [Enable test suite (requires check) @<:@default=yes@:>@])],
  [], [enable_testsuite=yes])

AC_ARG_ENABLE(debug,
	[  --enable-debug          turn on debugging],
	[CFLAGS="-g -O0 -Wall -DDEBUG ${CFLAGS}"],
	[CFLAGS="-O2 -Wall ${CFLAGS}"])

dnl Check for infiniband verbs
AC_ARG_ENABLE(verbs, AC_HELP_STRING([--disable-verbs],
		    [disable verbs (IBV) support (default is autodetect)]),
		    [enable_verbs=$enableval],
		    [enable_verbs=try])

if test "$enable_verbs" != "no" ; then
        AH_TEMPLATE(HAVE_VERBS, Whether or not ibverbs is installed on the system)
        AC_CHECK_LIB(ibverbs, main, [
                AC_CHECK_HEADERS([infiniband/verbs.h rdma/rdma_cma.h], [
                        VERBS_LIBS="-libverbs -lrdmacm"
                        AC_SUBST(VERBS_LIBS)
                        have_verbs=yes
                ])
        ])

        if test "x$have_verbs" == "xyes"; then
                AC_DEFINE(HAVE_VERBS, 1, Build with VERBS support)
        else
		AC_MSG_ERROR([IB/RDMA Verbs not found.  Try installing OFED or libibverbs-devel and librdmacm-devel])
	fi
fi
AM_CONDITIONAL(HAVE_VERBS, test x$have_verbs = xyes)

dnl Check for uGNI
AC_ARG_ENABLE([ugni], AC_HELP_STRING([--disable-ugni],
			[disable UGNI support (default is autodetect)]),
			[enable_ugni=$enableval],
			[enable_ugni=try])

if test "$enable_ugni" != "no" ; then
   AH_TEMPLATE(HAVE_UGNI, Whether or not UGNI is installed on the system)
   AC_MSG_CHECKING(for UGNI with pkg-config)
   `pkg-config --exists cray-ugni`
   have_ugni=`echo $?`
   if test $have_ugni -eq 0; then
      AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_UGNI, 1, Build with UGNI support)
      AC_SUBST(UGNI_CFLAGS, `pkg-config --cflags cray-ugni`)
      AC_SUBST(UGNI_LIBS, `pkg-config --libs cray-ugni`)
      have_ugni=yes
   else
      AC_MSG_RESULT(no)
      have_ugni=no
   fi
fi
AM_CONDITIONAL(HAVE_UGNI, test x$have_ugni = xyes)

dnl Check for mpich2
AC_ARG_ENABLE([mpich2], AC_HELP_STRING([--disable-mpich2],						
                    	[disable MPICH2 support (default is autodetect)]),
                    	[enable_mpich2=$enableval],
                    	[enable_mpich2=try])

if test "$enable_mpich2" != "no" ; then
   AH_TEMPLATE(HAVE_MPICH2, Whether or not MPICH2 is installed on the system)
   AC_CHECK_LIB([mpich2], [MPI_Init], [have_mpich2_lib=yes], [have_mpich2_lib=no])
   AC_CHECK_HEADER([mpich2/mpi.h], [have_mpich2=yes], [have_mpich2=no], [])

   if test "$have_mpich2" == "yes"; then
      	  if test -f /usr/include/mpich2/mpi.h; then
   	  	 AC_SUBST(MPI_CFLAGS, "-I/usr/include/mpich2")
		 have_mpich2=yes
	  elif test -f /usr/local/include/mpich2/mpi.h; then
		 AC_SUBST(MPI_CFLAGS, "-I/usr/local/include/mpich2")
		 have_mpich2=yes
	  fi
	  AC_SUBST(MPI_LIBS, "-L/usr/lib/mpich2/lib/ -L/usr/local/mpich2/lib -lmpich -lmpl")
	  AC_DEFINE(HAVE_MPICH2,, Build with MPICH2 support)
   fi
fi
AM_CONDITIONAL(HAVE_MPICH2, test x$have_mpich2 = xyes)

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
# --with-options, see config/photon_with_pkg.m4 for details
# ---------------------------------------------------------------------------
PHOTON_WITH_PKG([check],[check],[build with check unit tests],[no],[CHECK])
AS_IF([test "x$enable_testsuite" != xno -a "x$with_check" != xno],
  [enable_tests=yes],
  [enable_tests=no])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])

AC_CONFIG_FILES([
	Makefile
	src/util/Makefile
	src/Makefile
	test/Makefile
])

AS_IF([test "x$enable_tests" != xno],
  [AC_CONFIG_FILES([tests/Makefile])])

AC_OUTPUT

AC_SUBST(THREAD_LIBS)

cat << EOF
Config files successfully generated by ./configure $ac_configure_args !

  Install prefix     : $prefix
    
  To build and install, run:
     $ make
     $ make install

EOF
