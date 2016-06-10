# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_USERSPACE_RCU([pkg-config])
# ------------------------------------------------------------------------------
#
# Appends
#   LIBHPX_CPPFLAGS
#   LIBHPX_LIBS
#   HPX_PC_REQUIRES_PKGS
#
# Defines
#   URCU_INLINE_SMALL_FUNCTIONS
# ------------------------------------------------------------------------------
AC_DEFUN([_HPX_CONTRIB_USERSPACE_RCU], [
 contrib=$1
 
 HPX_MERGE_STATIC_SHARED([USERSPACE_RCU_CARGS])
 ACX_CONFIGURE_DIR([$contrib], [$contrib], ["$USERSPACE_RCU_CARGS"])

 # add the la dependency and include path for our libhpx build
 LIBHPX_LIBADD="$LIBHPX_LIBADD \$(top_builddir)/$contrib/liburcu-qsbr.la \$(top_builddir)/$contrib/liburcu-cds.la"
 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_srcdir)/$contrib/ -I\$(top_builddir)/$contrib/"

 # expose the libffi package as a public dependency to clients
 HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS liburcu-qsbr liburcu-cds"

 AC_DEFINE([URCU_INLINE_SMALL_FUNCTIONS], [1], [Optimize URCU as far as license allows])
])

AC_DEFUN([HPX_CONFIG_USERSPACE_RCU], [
 pkg=$1
 _HPX_CONTRIB_USERSPACE_RCU($pkg)
])
