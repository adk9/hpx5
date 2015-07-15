# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_HOST
#
# Perform host-specific work here.
# ------------------------------------------------------------------------------
AC_DEFUN([_HPX_DO_LINUX], [        
# l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
 AS_CASE([$host_cpu],
   [arm*], [l1d_linesize=32],
           [l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`])
 pagesize=`getconf PAGESIZE`

 AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
 LIBHPX_LIBS="$LIBHPX_LIBS -lrt"
 HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lrt"
])

AC_DEFUN([_HPX_DO_DARWIN], [
 l1d_linesize=`sysctl -n hw.cachelinesize`
 pagesize=`getconf PAGESIZE`
])

AC_DEFUN([HPX_CONFIG_HOST], [
 AS_CASE([$host_os],
    [linux*], [_HPX_DO_LINUX],
   [darwin*], [_HPX_DO_DARWIN],
              [AC_MSG_ERROR([Unsupported Host OS $host_os])])

 AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
 AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], [$pagesize], [OS Memory Page Size])
])
