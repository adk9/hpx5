AC_PREREQ([2.63])
AC_INIT([photon], [0.1a], [photon@stout.incntre.iu.edu])
AM_CONFIG_HEADER(include/config.h)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar parallel-tests -Wall -Werror])
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
                AC_DEFINE(HAVE_VERBS,, Build with VERBS support)
        else
		AC_MSG_ERROR([IB/RDMA Verbs not found.  Try installing OFED or libibverbs-devel and librdmacm-devel])
	fi
fi
AM_CONDITIONAL(HAVE_VERBS, test x$have_verbs = xyes)

dnl Check for uGNI and DMAPP
dnl TODO

dnl Check for mpich2
AC_ARG_ENABLE([mpich2], AC_HELP_STRING([--disable-mpich2],						
                    	[disable MPICH2 support (default is autodetect)]),
                    	[enable_mpich2=$enableval],
                    	[enable_mpich2=try])

if test "$enable_mpich2" != "no" ; then
   AH_TEMPLATE(HAVE_MPICH2, Whether or not MPICH2 is installed on the system)
   AC_CHECK_LIB([mpich2], [main], [have_mpich2_lib=yes], [have_mpich2_lib=no])
   AC_CHECK_HEADER([mpich2/mpi.h], [have_mpich2=yes], [have_mpich2=no], [])

   if test "$have_mpich2" == "no"; then
   	  if test -f /usr/include/mpich2/mpi.h; then
   	  	 AC_SUBST(MPI_CFLAGS, "-I/usr/include/mpich2")
		 have_mpich2=yes
	  elif test -f /usr/local/include/mpich2/mpi.h; then
		 AC_SUBST(MPI_CFLAGS, "-I/usr/local/include/mpich2")
		 have_mpich2=yes
	  fi
   fi
   
   if test "x$have_mpich2" == "xyes"; then
   	  	AC_DEFINE(HAVE_MPICH2,, Build with MPICH2 support)
      	AC_SUBST(MPI_LIBS, "-lmpich2")
   else
		AC_MSG_WARN([MPICH2 not found.])
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
   AC_MSG_CHECKING(for MPI libs in $with_mpi_lib with $LDFLAGS)
   save_LDFLAGS=$LDFLAGS
   LDFLAGS="-L$with_mpi_lib"
   AC_CHECK_LIB([mpi], [MPI_Init], [AC_SUBST(MPI_LIBS, "-L$with_mpi_lib -lmpi")],
   					   		   	   [AC_CHECK_LIB([mpich], [MPI_Init],
					   						 			  [AC_SUBST(MPI_LIBS, "-L$with_mpi_lib -lmpich")],
					   						 			  [AC_MSG_ERROR(No MPI libs found!)],
														  [-lmpl -lpthread])],
                                   [-lmpl -lpthread])
   LDFLAGS=$save_LDFLAGS
fi

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	test/Makefile
])
AC_OUTPUT

AC_SUBST(THREAD_LIBS)

cat << EOF
Config files successfully generated by ./configure $ac_configure_args !

  Install prefix     : $prefix
    
  To build and install, run:
     $ make
     $ make install

EOF
