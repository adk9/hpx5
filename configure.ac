# Copyright (c) 2013, Trustees of Indiana University 
# All rights reserved.

# This software may be modified and distributed under the terms of
# the BSD license.  See the COPYING file for details.

# This software was created at the Indiana University Center for
# Research in Extreme Scale Technologies (CREST).

AC_PREREQ([2.63])
AC_INIT([hpx], [5.0.0a1], [hpx@indiana.edu])
CLEANFILES="*~ .\#* .gdb_history"
AC_SUBST(CLEANFILES)

AC_CONFIG_SRCDIR([include/hpx.h])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([1.9 dist-bzip2 subdir-objects foreign tar-ustar -Wall -Werror])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_PREREQ([2.2.6])
LT_INIT

AC_LANG([C])

# Checks for programs.
AC_PROG_CC([icc gcc])
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AM_PROG_AS

#AS_IF([test "x$CC" = "icc"], [CFLAGS += "-no-gcc"])

dnl ---------------------------------------------------------------------------
dnl NB: Don't check for anything that we're not actually prepared to deal with
dnl     in our source code. Add these commented-out checks later if we come up
dnl     with platform-independent code that actually requires the result of
dnl     these checks.
dnl ---------------------------------------------------------------------------

dnl AC_CHECK_LIB([m], [pow])
dnl AC_CHECK_LIB([pthread], [pthread_self])

# Checks for header files.
dnl AC_CHECK_HEADERS([limits.h mach/mach.h stdbool.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
dnl AC_C_INLINE
dnl AC_TYPE_INT16_T
dnl AC_TYPE_INT32_T
dnl AC_TYPE_INT64_T
dnl AC_TYPE_INT8_T
dnl AC_TYPE_SIZE_T
dnl AC_TYPE_UINT16_T
dnl AC_TYPE_UINT32_T
dnl AC_TYPE_UINT64_T
dnl AC_TYPE_UINT8_T
dnl AC_C_CONST
dnl AC_TYPE_PID_T

# Checks for library functions.
dnl AC_FUNC_FORK
dnl AC_FUNC_MALLOC
dnl AC_FUNC_MMAP
dnl AC_FUNC_REALLOC
dnl AC_FUNC_STRERROR_R
dnl AC_CHECK_FUNCS([clock_gettime memset pow])


dnl ---------------------------------------------------------------------------
dnl This section declares all of the HPX-specific --enable options. These are
dnl entirely obvious and don't have extra comments.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE([pedantic],
    [AS_HELP_STRING([--enable-pedantic],
        [Enable -pedantic @<:@default=yes@:>@])],
    [], [enable_pedantic=yes])
AS_IF([test "x$enable_pedantic" != xno], AC_SUBST([HPX_PEDANTIC], [-pedantic]))

AC_ARG_ENABLE([wall],
    [AS_HELP_STRING([--enable-wall], [Enable -Wall @<:@default=yes@:>@])],
    [], [enable_wall=yes])
AS_IF([test "x$enable_wall" != xno], AC_SUBST([HPX_W_ALL], [-Wall]))

AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [Enable -Werror @<:@default=yes@:>@])],
    [], [enable_werror=yes])
AS_IF([test "x$enable_werror" != xno], AC_SUBST([HPX_W_ERROR], [-Werror]))

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [Enable debug code @<:@default=no@:>@])],
    [], [enable_debug=no])

dnl This is combined with the --with-check option to compute an "enable_docs"
dnl variable that we use to actually add doc support.
AC_ARG_ENABLE([testsuite],
    [AS_HELP_STRING([--enable-testsuite],
        [Enable test suite (requires check) @<:@default=yes@:>@])],
    [], [enable_testsuite=yes])


dnl ---------------------------------------------------------------------------
dnl This section declares all of the HPX-specific --with options. Unless
dnl otherwise specified we rely on pcg-config for all of these. If the user
dnl provides --with-option without specifying a PKGS (pkg-config string), then
dnl we provide a default.
dnl
dnl The goal of most of these is to compute CFLAGS and LIBS required by various
dnl Makefile.am files for building.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([papi],
    [AS_HELP_STRING([--with-papi[[=PKGS]]],
        [build with profiling @<:@default=no@:>@])],
    [], [with_papi=no])
    
AC_ARG_WITH([hwloc],
     [AS_HELP_STRING([--with-hwloc[[=PKGS]]],
         [build with topology awareness @<:@default=no@:>@])],
     [], [with_hwloc=no])

AC_ARG_WITH([tcmalloc],
    [AS_HELP_STRING([--with-tcmalloc[[=PKGS]]],
        [build with explicit tcmalloc @<:@default=no@:>@])],
    [], [with_tcmalloc=no])

AC_ARG_WITH([check],
    [AS_HELP_STRING([--with-check[[=PKGS]]],
        [build unit testing @<:@default=yes@:>@])],
    [], [with_check=yes])

AC_ARG_WITH([doxygen],
    [AS_HELP_STRING([--with-doxygen],
        [build documentation @<:@default=no@:>@])],
    [], [with_doxygen=yes])

dnl We can get access to MPI in one of three ways.
dnl
dnl 1) It may be that the application developer is using a CC like mpicc that
dnl    has MPI builtin. Then, all they need to do is set --with-mpi=yes in order
dnl    to enable it (in fact, in this case --without-mpi won't do them any
dnl    good). 
dnl
dnl 2) They may have a package installed that supports pkg-config, like the
dnl    MacPorts installation for OpenMPI. In that case, they can pass the
dnl    package identifier to --with-mpi, --with-mpi=ompi say.
dnl
dnl 3) The may have an MPI implementation installed that the compiler doesn't
dnl    know about, but that isn't visible to pkg-config. In that case, they can
dnl    set the include and lib directories using --with-mpi-include and/or
dnl    --with-mpi-lib.
AC_ARG_WITH([mpi],
    [AS_HELP_STRING([--with-mpi[[=PKGS]]],
        [build with MPI @<:@default=yes@:>@])],
    [], [with_mpi=yes])

AC_ARG_WITH([mpi_include],
    [AS_HELP_STRING([--with-mpi-include=dir], [include path for MPI])],
     [], [with_mpi_include=no])

AC_ARG_WITH([mpi_lib],
    [AS_HELP_STRING([--with-mpi-lib=dir], [library path for MPI])],
    [], [with_mpi_lib=no])

dnl make sure that arguments are passed
AS_IF([test "x$with_mpi_include" = xyes],
    [AC_MSG_ERROR(--with-mpi-include requires a path argument, use --with-mpi[[=yes]] if the MPI include path is in your path)])
AS_IF([test "x$with_mpi_lib" = xyes],
    [AC_MSG_ERROR(--with-mpi-lib requires a path argument, use --with-mpi[[=yes]] if the MPI library path is in your path)])

dnl --with-mpi-include or --with-mpi-lib "force" --with-mpi=yes, if it's not set
AS_IF([test "x$with_mpi" = xno -a "x$with_mpi_include" != xno], [with_mpi=yes])
AS_IF([test "x$with_mpi" = xno -a "x$with_mpi_lib" != xno], [with_mpi=yes])

dnl We can get access to Photon in one of three ways.
dnl
dnl 1) It may be that the application developer has Photon in their path
dnl    already, in that case, they just need to use --with-photon[=yes]
dnl
dnl 2) They may have a Photon package installed that supports pkg-config. In
dnl    this case they can pass the package name to --with-photon.
dnl
dnl 3) The may have an Photon implementation installed that the compiler doesn't
dnl    know about, but that isn't visible to pkg-config. In that case, they can
dnl    set the include and lib directories using --with-photon-include and/or
dnl    --with-photon-lib.
AC_ARG_WITH([photon],
    [AS_HELP_STRING([--with-photon],
        [build with Photon[[=PKGS]] @<:@default=no@:>@])],
    [], [with_photon=no])

AC_ARG_WITH([photon_include],
    [AS_HELP_STRING([--with-photon-include=dir], [include path for Photon])],
     [], [with_photon_include=no])

AC_ARG_WITH([photon_lib],
    [AS_HELP_STRING([--with-photon-lib=dir], [library path for Photon])],
    [], [with_photon_lib=no])

dnl make sure that arguments are passed
AS_IF([test "x$with_photon_include" = xyes],
    [AC_MSG_ERROR(--with-photon-include requires a path argument, use --with-photon[[=yes]] if the PHOTON include path is in your path)])
AS_IF([test "x$with_photon_lib" = xyes],
    [AC_MSG_ERROR(--with-photon-lib requires a path argument, use --with-photon[[=yes]] if the PHOTON library path is in your path)])

dnl --with-photon-include or --with-photon-lib "force" --with-photon=yes, if it's not set
AS_IF([test "x$with_photon" = xno -a "x$with_photon_include" != xno], [with_photon=yes])
AS_IF([test "x$with_photon" = xno -a "x$with_photon_lib" != xno], [with_photon=yes])


dnl We can get access to GLIB in one of three ways.
dnl
dnl 1) It may be that glib is just in the path, in which case we don't need to
dnl    do anything special.
dnl
dnl 2) They may have a package installed that supports pkg-config. In that case,
dnl    they can pass the package identifier to --with-glib, --with-glib=glib-2.0
dnl    say.
dnl
dnl 3) The may have an GLIB implementation installed that the compiler doesn't
dnl    know about, but that isn't visible to pkg-config. In that case, they can
dnl    set the include and lib directories using --with-glib-include and/or
dnl    --with-glib-lib.
AC_ARG_WITH([glib],
    [AS_HELP_STRING([--with-glib[[=PKGS]]],
        [build with GLIB-2.0 @<:@default=yes@:>@])],
    [], [with_glib=yes])

AC_ARG_WITH([glib_include],
    [AS_HELP_STRING([--with-glib-include=dir], [include path for GLIB-2.0])],
     [], [with_glib_include=no])

AC_ARG_WITH([glib_lib],
    [AS_HELP_STRING([--with-glib-lib=dir], [library path for GLIB-2.0])],
    [], [with_glib_lib=no])

dnl make sure that arguments are passed
AS_IF([test "x$with_glib_include" = xyes],
    [AC_MSG_ERROR(--with-glib-include requires a path argument, use --with-glib[[=yes]] if the GLIB include path is in your path)])
AS_IF([test "x$with_glib_lib" = xyes],
    [AC_MSG_ERROR(--with-glib-lib requires a path argument, use --with-glib[[=yes]] if the GLIB library path is in your path)])

dnl --with-glib-include or --with-glib-lib "force" --with-glib=yes, if it's not set
AS_IF([test "x$with_glib" = xno -a "x$with_glib_include" != xno], [with_glib=yes])
AS_IF([test "x$with_glib" = xno -a "x$with_glib_lib" != xno], [with_glib=yes])

AC_ARG_WITH([crypto],
    [AS_HELP_STRING([--with-crypto[[=PKGS]]],
         [use libcrypto for string hashing @<:@default=yes@:>@])],
    [], [with_crypto=no])


dnl ---------------------------------------------------------------------------
dnl Here, we provide default packages for any --with option for which we have a
dnl default. This cleans up the variables for the circumstances where the user
dnl only said (e.g.,) --with-doxygen.
dnl
dnl NB: there is no default for --with-mpi or --with-photon, because we provide
dnl     the -include,-lib interface for them.
dnl ---------------------------------------------------------------------------

AS_IF([test "x$with_papi" = xyes],      with_papi=papi)
AS_IF([test "x$with_hwloc" = xyes],     with_hwloc=hwloc)
AS_IF([test "x$with_tcmalloc" = xyes],  with_tcmalloc=libtcmalloc)
AS_IF([test "x$with_check" = xyes],     with_check="check >= 0.9.4")
AS_IF([test "x$with_crypto" = xyes],    with_crypto=libcrypto)


dnl ---------------------------------------------------------------------------
dnl For each optional package, compute the CFLAGS and LIBS that we need for
dnl compilation. Some of these use the PKG_CHECK_MODULES functionality to do
dnl this, while some of these are hardcoded.
dnl ---------------------------------------------------------------------------

AS_IF([test "x$with_crypto" != xno],
    [PKG_CHECK_MODULES([CRYPTO], [$with_crypto])
     AC_SUBST(CRYPTO_CFLAGS)
     AC_SUBST(CRYPTO_LIBS)
     AC_SUBST(CRYPTO_PKG, [$with_crypto])])

AS_IF([test "x$with_papi" != xno],
    [PKG_CHECK_MODULES([PAPI], [$with_papi])
     AC_SUBST(PAPI_CFLAGS)
     AC_SUBST(PAPI_LIBS)
     AC_SUBST(PAPI_PKG, [$with_papi])])

AS_IF([test "x$with_hwloc" != xno],
    [PKG_CHECK_MODULES([HWLOC], [$with_hwloc])
     AC_SUBST(HWLOC_CFLAGS)
     AC_SUBST(HWLOC_LIBS)
     AC_SUBST(HWLOC_PKG, [$with_hwloc])])
    
AS_IF([test "x$with_tcmalloc" != xno],
    [PKG_CHECK_MODULES([TCMALLOC], [$with_tcmalloc])
     AC_SUBST(TCMALLOC_CFLAGS)
     AC_SUBST(TCMALLOC_LIBS)
     AC_SUBST(TCMALLOC_PKG, [$with_tcmalloc])])    

dnl Only pkg-config MPI if there is a package supplied for it, otherwise, use
dnl the arguments passed to the --with-mpi-include and --with-mpi-lib
dnl options. If the user uses a --with-mpi-lib option, then we assume that they
dnl need -lmpi explicitly.
AS_IF([test "x$with_mpi" != xno],
    [AS_IF([test "x$with_mpi" != xyes],
         [PKG_CHECK_MODULES([MPI], [$with_mpi])
          AC_SUBST(MPI_CFLAGS)
          AC_SUBST(MPI_LIBS)
          AC_SUBST(MPI_PKG, [$with_mpi])
          AS_IF([test "x$with_mpi_include" != xno -o "x$with_mpi_lib" != xno],
              [AC_MSG_WARN([--with-mpi=$with_mpi overrides --with-mpi-include and --with-mpi-lib])])],
         [AS_IF([test "x$with_mpi_include" != xno],
              [AC_SUBST(MPI_CFLAGS, ["-I$with_mpi_include"])])
          AS_IF([test "x$with_mpi_lib" != xno],
              [AC_SUBST(MPI_LIBS, ["-L$with_mpi_lib -lmpi"])])])])

dnl Only pkg-config GLIB if there is a package supplied for it, otherwise, use
dnl the arguments passed to the --with-glib-include and --with-glib-lib
dnl options.
AS_IF([test "x$with_glib" != xno],
    [AS_IF([test "x$with_glib" != xyes],
         [PKG_CHECK_MODULES([GLIB], [$with_glib])
          AC_SUBST(GLIB_CFLAGS)
          AC_SUBST(GLIB_LIBS)
          AC_SUBST(GLIB_PKG, [$with_glib])
          AS_IF([test "x$with_glib_include" != xno -o "x$with_glib_lib" != xno],
              [AC_MSG_WARN([--with-glib=$with_glib overrides --with-glib-include and --with-glib-lib])])],
         [AS_IF([test "x$with_glib_include" != xno],
              [AC_SUBST(GLIB_CFLAGS, ["-I$with_glib_include"])])
          AS_IF([test "x$with_glib_lib" != xno],
              [AC_SUBST(GLIB_LIBS, ["-L$with_glib_lib -lglib-2.0"])])])])

dnl Only pkg-config PHOTON if there is a package supplied for it, otherwise, use
dnl the arguments passed to the --with-photon-include and --with-photon-lib
dnl options. If the user uses a --with-photon-lib option, then we assume that they
dnl need -lphoton explicitly.
AS_IF([test "x$with_photon" != xno],
    [AS_IF([test "x$with_photon" != xyes],
         [PKG_CHECK_MODULES([PHOTON], [$with_photon])
          AC_SUBST(PHOTON_CFLAGS)
          AC_SUBST(PHOTON_LIBS)
          AC_SUBST(PHOTON_PKG, [$with_photon])
          AS_IF([test "x$with_photon_include" != xno -o "x$with_photon_lib" != xno],
              [AC_MSG_WARN([--with-photon=$with_photon overrides --with-photon-include and --with-photon-lib])])],
         [AS_IF([test "x$with_photon_include" != xno],
              [AC_SUBST(PHOTON_CFLAGS, ["-I$with_photon_include"])])
          AS_IF([test "x$with_photon_lib" != xno],
              [AC_SUBST(PHOTON_LIBS, ["-L$with_photon_lib -lphoton -libverbs -lrdmacm"])])])])

dnl Only check the "check" package if we're trying to build the testsuite. Also
dnl don't fail configuration if we can't find check, just print a nice warning
dnl and move along. Define enable_tests correctly.
AS_IF([test "x$enable_testsuite" != xno -a "x$with_check" != xno],
    [PKG_CHECK_MODULES([CHECK], [$with_check],
        [enable_tests=yes
         AC_SUBST(CHECK_CFLAGS)
         AC_SUBST(CHECK_LIBS)],
        [enable_tests=no
         AC_MSG_WARN([Check not found. Install Check to enable the test-suite.])])],
    [enable_tests=no])

dnl Don't need CFLAGS or LIBS for doxygen. Just define "enable_docs" correctly
dnl and move along if we can't find doxygen.
AS_IF([test "x$with_doxygen" != xno], [AC_CHECK_PROGS([DOXYGEN], [doxygen])])
AS_IF([test "x$DOXYGEN" != x], [enable_docs=yes],
    [enable_docs=no
     AC_MSG_WARN([Doxygen not found, disabling source documentation build.])])

    
dnl ---------------------------------------------------------------------------
dnl Perform host-specific work here.
dnl ---------------------------------------------------------------------------

AS_CASE([$host_os],
    [linux*],   [AC_SUBST([PLATFORM_LIBS], [-lrt])
                 l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
                 pagesize=`getconf PAGESIZE`],
    [darwin*],  [l1d_linesize=`sysctl -n hw.cachelinesize`
                 pagesize=`getconf PAGESIZE`],
    [solaris*], [l1d_linesize=`prtpicl -v -c cpu | \
                                     grep l1-dcache-line-size | uniq | \
                                     awk '{print $2}'`]
    [AC_MSG_WARN([Unexpected Host OS $host_os, using defaults])
     l1d_linesize=128
     pagesize=4096])
    
dnl ---------------------------------------------------------------------------
dnl In addition to CFLAGS and LIBS for the build process, some of the with
dnl packages result in different source-code logic through #defines. Generate
dnl them now.
dnl
dnl NB: We already know we passed any PKG_CHECK_MODULES for these because the
dnl     default behavior that we're using above is to fail. If we change the
dnl     default, then we would use the _PKG_ERRORS variables to condition this
dnl     section.
dnl
dnl NB: Please only #define things that we actually use.
dnl ---------------------------------------------------------------------------

AS_IF([test "x$with_crypto" != xno],
    AC_DEFINE([HAVE_CRYPTO], [1], [Enable libcrypto for string hashing]))
    
AS_IF([test "x$with_tcmalloc" != xno],
    AC_DEFINE([HAVE_TCMALLOC], [1], [Enable TCMALLOC support]))
    
AS_IF([test "x$with_hwloc" != xno],
    AC_DEFINE([HAVE_HWLOC], [1], [Enable hwloc support]))

AS_IF([test "x$with_mpi" != xno],
    AC_DEFINE([HAVE_MPI], [1], [Enable MPI support]))

AS_IF([test "x$with_photon" != xno],
    AC_DEFINE([HAVE_PHOTON], [1], [Enable Photon support]))

AS_IF([test "x$with_mpi" != xno -o "x$with_photon" != xno])
    AC_DEFINE([HAVE_NETWORK], [1], [Enable networking support])

AS_IF([test "x$with_glib" != xno],
    AC_DEFINE([HAVE_GLIB], [1], [Enable GLIB stuff]))
    
AS_IF([test "x$enable_debug" != xno],
    AC_DEFINE([HAVE_DEBUG], [1], [Enable debugging stuff]))


dnl ---------------------------------------------------------------------------
dnl Any other custom #defines go here.
dnl
dnl NB: Please only #define things that we actually use.
dnl ---------------------------------------------------------------------------
AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])

AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], [$pagesize], [OS Memory Page Size])

    
dnl ---------------------------------------------------------------------------
dnl Set automake conditionals for use in Makefile.am settings.
dnl
dnl NB: Please only declare conditionals that we have actual uses of.
dnl ---------------------------------------------------------------------------

AM_CONDITIONAL([CPU_X86_64], [test "x$host_cpu" = xx86_64])
AM_CONDITIONAL([HAVE_MPI], [test "x$with_mpi" != xno])
AM_CONDITIONAL([HAVE_PHOTON], [test "x$with_photon" != xno])
AM_CONDITIONAL([HAVE_NETWORK], [test "x$with_mpi" != xno -o "x$with_photon" != xno])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" != xno])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != xno])


dnl ---------------------------------------------------------------------------
dnl Use autoconf to configure all of the Makefiles that need to be
dnl processed. First, add anything that weill be processed unconditionally, and
dnl then at any configuration-specific files.
dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([
  Makefile
  libsync/Makefile
  src/Makefile
  src/libhpx.pc
  include/Makefile
  examples/Makefile
  examples/hpx/Makefile	
  examples/thread/Makefile
  examples/network/Makefile
])

AS_IF([test "x$enable_docs" != xno],
    AC_CONFIG_FILES([docs/Doxyfile docs/Makefile]))

AS_IF([test "x$enable_tests" = xyes],
    AC_CONFIG_FILES([tests/Makefile]))
    
AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl Output a summary of the results.
dnl ---------------------------------------------------------------------------

AS_IF([test "x$with_mpi" != xno], [networks="MPI"], [])
AS_IF([test "x$with_photon" != xno], [networks="$networks Photon"], [])
      
echo ""
echo "================================================"
echo " HPX Build Configuration"
echo "================================================"
echo "               Host CPU : $host_cpu"
echo "                Host OS : $host_os"
echo "         String Hashing : $with_crypto"
echo "               Networks : $networks"
echo "              Profiling : $with_papi"
echo "     Topology Awareness : $with_hwloc"
echo "  Thread-Caching Malloc : $with_tcmalloc"
echo "          Documentation : $enable_docs"
echo "             Test Suite : $enable_tests"
echo "================================================"

