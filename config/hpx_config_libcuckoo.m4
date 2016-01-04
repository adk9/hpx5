# -*- autoconf -*---------------------------------------------------------------
# HPX_CONFIG_LIBCUCKOO([path])
#
# Variables
#   have_libcuckoo
#   build_libcuckoo
#
# Defines
#   HAVE_LIBCUCKOO
#
# Appends
#   LIBHPX_CPPFLAGS
#   LIBHPX_LIBADD
#   HPX_PC_PRIVATE_LIBS
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONFIG_LIBCUCKOO], [
 contrib=$1
 required=$2

 AS_IF([test "x$required" == xyes], 
   [HPX_MERGE_STATIC_SHARED([LIBCUCKOO_CARGS]) 
    ACX_CONFIGURE_DIR([$contrib], [$contrib], ["$LIBCUCKOO_CARGS"])
    LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_srcdir)/$contrib/src/"
    LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_srcdir)/$contrib/cityhash-1.1.1/src/"
    LIBHPX_LIBADD="$LIBHPX_LIBADD \$(top_builddir)/$contrib/cityhash-1.1.1/src/libcityhash.la"
    HPX_PC_PRIVATE_LIBS="$HPX_PC_PRIVATE_LIBS -lcityhash"
    AC_DEFINE([HAVE_LIBCUCKOO], [1], [We have the libcuckoo hash table])
    have_libcuckoo=yes
    build_libcuckoo=yes])
])
