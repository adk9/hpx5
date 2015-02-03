# -*- autoconf -*---------------------------------------------------------------
# HPX_PE_ENV()
# ------------------------------------------------------------------------------
# Sets
#   $hpx_pe_env
#   $hpx_pe_env_cflags
#   $hpx_pe_env_cflags_pedantic
#   $hpx_pe_env_cflags_wall
#   $hpx_pe_env_cflags_werror
#   $hpx_pe_env_cppflags
#   $hpx_pe_env_ccasflags
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_PE_ENV],
  [AS_IF([test -z "$PE_ENV"],
    [AS_CASE([$CC],
      [craycc*], [AC_SUBST([PE_ENV],["CRAY"])],
        [pgcc*], [AC_SUBST([PE_ENV],["PGI"])],
         [icc*], [AC_SUBST([PE_ENV],["INTEL"])],
         [gcc*], [AC_SUBST([PE_ENV],["GNU"])],
                 [AC_SUBST([PE_ENV],["UNKNOWN"])])])

   AC_MSG_CHECKING([for programming environment])
   AS_CASE([$PE_ENV],
        [CRAY*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["cc"])])
                  hpx_pe_env_cppflags="‐h noomp"
                  hpx_pe_env_cflags="-h nomessage=1254"
                  hpx_pe_env_cflags_pedantic="" # LD: Can't be pedantic yet---no atomics
                  hpx_pe_env_cflags_wall=""
                  hpx_pe_env_cflags_werror=""
                  hpx_pe_env_ccasflags="-h gnu -h nomessage=1254" # ADK: We need this for ASM labels.
                  hpx_pe_env="CRAY"],
         [PGI*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["pgcc"])])
                  hpx_pe_env="PGI"],
       [INTEL*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["icc"])])
                  hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_cflags_werror="-Werror"
                  hpx_pe_env="INTEL"],
         [GNU*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["gcc"])])
                  hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_cflags_werror="-Werror"
                  hpx_pe_env_libs="-lrt"
                  hpx_pe_env="GNU"],
                 [hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_cflags_werror="-Werror"
                  hpx_pe_env_libs="-lrt"
                  hpx_pe_env="GNU"])
   AC_MSG_RESULT([$hpx_pe_env])])
