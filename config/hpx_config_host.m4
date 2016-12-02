# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_HOST
#
# Perform host-specific work here.
#
# Variables
#   l1d_linesize
#   pagesize
#   libffi_contrib_dir
#
# Substitutes
#   LIBFFI_CONTRIB_DIR
#
# Defines
#   HPX_CACHELINE_SIZE
#   HPX_PAGE_SIZE
#
# Appends
#   HPX_APPS_LDADD
#   HPX_PC_PUBLIC_LIBS
# ------------------------------------------------------------------------------
AC_DEFUN([_HPX_DO_LINUX], [
 AS_CASE([$host_cpu],
   [x86_64], [HPX_CHECK_CMPXCHG16B
              l1d_linesize=`cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size`],
  [powerpc64le], [LIBHPX_CFLAGS="$LIBHPX_CFLAGS -fexceptions"
              HPX_APPS_CFLAGS="$HPX_APPS_CFLAGS -fexceptions"
              HPX_PC_CFLAGS="$HPX_PC_CFLAGS -fexceptions"
              l1d_linesize=64],
  [aarch64], [LIBHPX_CFLAGS="$LIBHPX_CFLAGS -fexceptions"
  	      HPX_APPS_CFLAGS="$HPX_APPS_CFLAGS -fexceptions"
  	      HPX_PC_CFLAGS="$HPX_PC_CFLAGS -fexceptions"
              HPX_AARCH64_GET_CACHELINESIZE
              AS_ECHO(["checking cache line size for aarch64... $l1d_linesize"])],
     [arm*], [LIBHPX_CFLAGS="$LIBHPX_CFLAGS -fexceptions"
  	      HPX_APPS_CFLAGS="$HPX_APPS_CFLAGS -fexceptions"
  	      HPX_PC_CFLAGS="$HPX_PC_CFLAGS -fexceptions"
              l1d_linesize=32],
             [l1d_linesize=`getconf LEVEL1_DCACHE_LINESIZE`])
 pagesize=`getconf PAGESIZE`
 jemalloc_default=yes

 # Linux platforms support ucru
 want_urcu=yes

 AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Define the POSIX version])
 HPX_APPS_LDADD="$HPX_APPS_LDADD -lrt"
 HPX_PC_CFLAGS="$HPX_PC_CFLAGS -D_POSIX_C_SOURCE=200809L"
 HPX_PC_PUBLIC_LIBS="$HPX_PC_PUBLIC_LIBS -lrt"
])

AC_DEFUN([_HPX_DO_DARWIN], [
 l1d_linesize=`sysctl -n hw.cachelinesize`
 pagesize=`getconf PAGESIZE`
 jemalloc_default=no

 # Darwin doesn't currently support urcu
 want_urcu=no
])

AC_DEFUN([HPX_CONFIG_HOST], [
 AS_CASE([$host_os],
    [linux*], [_HPX_DO_LINUX],
   [darwin*], [_HPX_DO_DARWIN],
              [AC_MSG_ERROR([Unsupported Host OS $host_os])])

 AS_CASE([$host_vendor],
      [k1om], [libffi_contrib_dir=libffi-mic],
              [libffi_contrib_dir=libffi])

 AC_SUBST([LIBFFI_CONTRIB_DIR], [$libffi_contrib_dir])
 AC_DEFINE_UNQUOTED([HPX_CACHELINE_SIZE], [$l1d_linesize], [Cacheline size])
 AC_SUBST(HPX_CACHELINE_SIZE, [$l1d_linesize])
 AC_DEFINE_UNQUOTED([HPX_PAGE_SIZE], [$pagesize], [OS Memory Page Size])
 AC_CHECK_HEADERS([execinfo.h])
])
