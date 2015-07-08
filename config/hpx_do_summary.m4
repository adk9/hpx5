# -*- autoconf -*---------------------------------------------------------------
# HPX_DO_SUMMARY
#
# Set automake conditionals for use in Makefile.am settings, HWLOC-style.
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_DO_SUMMARY], [

 # Compute some friendly strings
 AS_IF([test "x$have_mpi" == xyes], [networks="MPI"])
 AS_IF([test "x$have_photon" == xyes], [networks="Photon $networks"])

 AS_IF([test "x$have_jemalloc" == xyes], [allocator="jemalloc"])
 AS_IF([test "x$have_tbbmalloc" == xyes], [allocator="tbbmalloc"])

 AS_IF([test "x$enable_tests" == xyes], [tests="integration"])
 AS_IF([test "x$enable_lengthy_tests" == xyes], [tests="$tests lengthy"])

 echo ""
 echo "================================================"
 echo " HPX Build Configuration"
 echo "================================================"
 echo "               Host CPU : $host_cpu"
 echo "                Host OS : $host_os"
 echo "               Networks : $networks"
 echo "              Allocator : $allocator"
 echo "              Profiling : $enable_profiling"
 echo "         hugetlbfs heap : $have_hugetlbfs"
 echo "     Topology Awareness : $have_hwloc"
 echo "          Documentation : $enable_docs"
 echo "               Tutorial : $enable_tutorial"
 echo "             Test Suite : $tests"
 echo "         Extra CPPFLAGS : $hpx_extra_cppflags"
 echo "           Extra CFLAGS : $hpx_extra_cflags"
 echo "        Extra CCASFLAGS : $hpx_extra_ccasflags"
 echo "       Install location : $prefix"
 echo "================================================"
 
])
