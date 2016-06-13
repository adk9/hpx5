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

AC_DEFUN([_HAVE_URCU], [
 AC_DEFINE([URCU_INLINE_SMALL_FUNCTIONS], [1], [Optimize URCU as far as license allows])
 AC_DEFINE([HAVE_USERSPACE_RCU], [1], [We have the urcu libraries])
 have_userspace_rcu=yes
])

AC_DEFUN([_BUILD_URCU], [
 _HAVE_URCU
 build_userspace_rcu=yes
])

AC_DEFUN([_CONTRIB_URCU], [
 contrib=$1
 pkgs=$2
 
 HPX_MERGE_STATIC_SHARED([URCU_CARGS])
 ACX_CONFIGURE_DIR([$contrib], [$contrib], ["$URCU_CARGS"])
 _BUILD_URCU
 
 # add the la dependency and include path for our libhpx build
 LIBHPX_LIBADD="$LIBHPX_LIBADD \$(top_builddir)/$contrib/liburcu-qsbr.la \$(top_builddir)/$contrib/liburcu-cds.la"
 LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS -I\$(top_srcdir)/$contrib/ -I\$(top_builddir)/$contrib/"

 # expose the urcu packages as a public dependency to clients
 HPX_PC_REQUIRES_PKGS="$HPX_PC_REQUIRES_PKGS $pkgs"
])

# Check to see if userspace rcu is "just available" on the system.
AC_DEFUN([_LIB_URCU], [
 AC_CHECK_HEADER([urcu-qsbr.h], [
   AC_CHECK_LIB([liburcu-qsbr], [rcu_register_thread], [
     AC_CHECK_LIB([liburcu-cds], [cds_lfht_new],
      [_HAVE_URCU
       LIBHPX_LIBS="$LIBHPX_LIBS -lurcu-qsbr -lurcu-cds"
       HPX_PC_PUBLIC_LIBS="$HPX_PC_PUBLIC_LIBS -lurcu-qsbr -lurcu-cds"])])])
])

# Try and find a package for userspace rcu.
AC_DEFUN([_PKG_URCU], [
 pkgs=$1
 
 PKG_CHECK_MODULES([URCU], [$pkgs],
   [_HAVE_URCU
    LIBHPX_CPPFLAGS="$LIBHPX_CPPFLAGS $URCU_CFLAGS"
    LIBHPX_LIBS="$LIBHPX_LIBS $URCU_LIBS"
    HPX_PC_PRIVATE_PKGS="$HPX_PC_PRIVATE_PKGS $pkgs"],
   [have_userspace_rcu=no])
])

# handle the with_userspace_rcu option
AC_DEFUN([_WITH_URCU], [
 pkgs=$1 

 AS_IF([test "with_userspace_rcu" == xyes], [with_userspace_rcu=system])
 AS_IF([test "with_userspace_rcu" == xno], [with_userspace_rcu=contrib])
 
 AS_CASE($with_userspace_rcu,
   # just go ahead and build the contrib
   [contrib], [],
   
   # system means that we look for a library in the system path, or a
   # default-named pkg-config package
   [system], [_LIB_URCU
              AS_IF([test "x$have_userspace_rcu" != xyes], [_PKG_URCU($pkgs)])],

   # any other string is interpreted as a custom pkg-config package
   [_PKG_URCU($with_userspace_rcu)])
])

AC_DEFUN([HPX_CONFIG_USERSPACE_RCU], [
 contrib=$1
 pkgs="liburcu-qsbr liburcu-cds"
 
 AC_ARG_WITH(userspace_rcu,
   [AS_HELP_STRING([--with-userspace-rcu{=contrib,system,PKG}],
                   [How we find liburcu* @<:@default=system@:>@])],
   [], [with_userspace_rcu=system])

 _WITH_URCU($pkgs)

 AS_IF([test "x$have_userspace_rcu" != xyes], [_CONTRIB_URCU($contrib, $pkgs)])
])
