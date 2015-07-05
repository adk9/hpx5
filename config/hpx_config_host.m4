# ---------------------------------------------------------------------------
# HPX_CONFIG_HOST
# ---------------------------------------------------------------------------
# Perform host-specific work here.
# ---------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_HOST], [
 AS_CASE([$host_os],
   [linux*],
     [AS_CASE([$host_cpu],
       [arm*],   [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
                  l1d_linesize=32
                  pagesize=`getconf PAGESIZE`],
       [AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
        l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`
        l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`
        pagesize=`getconf PAGESIZE`])],
   [darwin*], [l1d_linesize=`sysctl -n hw.cachelinesize`
               pagesize=`getconf PAGESIZE`],
   [AC_MSG_WARN([Unexpected Host OS $host_os, using defaults])
    l1d_linesize=128
    pagesize=4096])

 AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
 AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], [$pagesize], [OS Memory Page Size])
])
