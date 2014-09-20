# -*- autoconf -*---------------------------------------------------------------
# HPX_OPENFLOW()
# ------------------------------------------------------------------------------
# Set up options related to openflow support.
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_OPENFLOW],
  [AC_ARG_ENABLE([floodlight],
     [AS_HELP_STRING([--enable-floodlight],
                     [Enable Floodlight controller support @<:@default=no@:>@])],
     [], [enable_floodlight=no])
     
   HPX_WITH_PKG([trema],[libtrema],[use the Trema OF controller API],[no],[TREMA])
   HPX_WITH_PKG([jansson],[jansson],[use libjansson for JSON parsing],[no],[JANSSON])
   HPX_WITH_PKG([curl],[libcurl],[use libcurl for OF REST calls],[no],[CURL])
   AS_IF([test "x$enable_floodlight" != xno],
     AS_IF([test "x$with_jansson" != xno -a "x$with_curl" != xno],
       [AC_DEFINE([HAVE_FLOODLIGHT],[1],[Enable Floodlight support])]))
        AM_CONDITIONAL([HAVE_FLOODLIGHT],
                       [test "xwith_jansson" != xno -a "x$with_curl" != xno])])

   
