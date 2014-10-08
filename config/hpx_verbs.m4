# -*- autoconf -*---------------------------------------------------------------
# HPX_WITH_VERBS()
# ------------------------------------------------------------------------------

AC_DEFUN([HPX_WITH_VERBS],[
  AH_TEMPLATE(HAVE_VERBS, Whether or not ibverbs is installed on the system)
    AC_CHECK_LIB([ibverbs], [main], [
      AC_CHECK_HEADERS([infiniband/verbs.h rdma/rdma_cma.h], [
                       VERBS_LIBS="-libverbs -lrdmacm"
                       AC_SUBST(VERBS_LIBS)
                       have_verbs=yes])], [
      PKG_CHECK_MODULES([VERBS], [$with_verbs], [],
        [AC_MSG_WARN([pkg-config could not find verbs=$with_verbs])
         AC_MSG_WARN([falling back to {VERBS_CFLAGS, VERBS_LIBS} variables])])
       AC_SUBST(VERBS_CFLAGS)
       AC_SUBST(VERBS_LIBS)
       AC_SUBST(VERBS_PKG, [$with_verbs])])

  AS_IF([test "x$have_verbs" == "xyes"],
    [AC_DEFINE(HAVE_VERBS, 1, Build with VERBS support)],
    [AC_MSG_ERROR([IB/RDMA Verbs not found.  Try installing OFED or libibverbs-devel and librdmacm-devel])])

  AM_CONDITIONAL([HAVE_VERBS], [test x$have_verbs = xyes])
])
