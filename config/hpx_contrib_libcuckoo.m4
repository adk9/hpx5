# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_LIBCUCKOO([path],[prefix])
# ------------------------------------------------------------------------------
# Sets
#   $prefix_libcuckoo_cppflags
#
# Substitutes
#   LIBCUCKOO_CPPFLAGS
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_LIBCUCKOO_CARGS], [Additional args passed to libcuckoo contrib])

AC_DEFUN([HPX_CONTRIB_LIBCUCKOO],
  [$2libcuckoo_cargs=" $HPX_LIBCUCKOO_CARGS"
   ACX_CONFIGURE_DIR([$1], [$1], [$$2libcuckoo_cargs])

   $2libcuckoo_cppflags="-I\$(top_srcdir)/$1/src/ -I\$(top_srcdir)/$1/cityhash-1.1.1/src/"
   AC_SUBST(LIBCUCKOO_CPPFLAGS, [$$2libcuckoo_cppflags])
   AC_SUBST(HPX_LIBCUCKOO_LIBS, ["\$(abs_top_builddir)/$1/cityhash-1.1.1/src/libcityhash.la"])])
