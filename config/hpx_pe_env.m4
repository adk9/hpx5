# -*- autoconf -*---------------------------------------------------------------
# HPX_PE_ENV()
# ------------------------------------------------------------------------------
# Sets
#   $hpx_pe_env
#   $hpx_pe_env_cflags
#   $hpx_pe_env_cflags_pedantic
#   $hpx_pe_env_cflags_wall
#   $hpx_pe_env_cppflags
#   $hpx_pe_env_ccasflags
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_PE_ENV],
  [AS_IF([test -z "$PE_ENV"],
    # Determine programming environment based on the compiler.
    [AS_CASE([$ax_cv_c_compiler_vendor],
        [cray*], [AC_SUBST([PE_ENV],["CRAY"])],
    [portland*], [AC_SUBST([PE_ENV],["PGI"])],
       [intel*], [AC_SUBST([PE_ENV],["INTEL"])],
         [gnu*], [AC_SUBST([PE_ENV],["GNU"])],
       [clang*], [AC_SUBST([PE_ENV],["CLANG"])],
                 [AC_SUBST([PE_ENV],["UNKNOWN"])])])

   AS_CASE([$host_os],
     [linux*], [PE_ENV_LIBS="-lrt"],
    [darwin*], [PE_ENV_LIBS=""],
               [PE_ENV_LIBS="-lrt"])

   AC_MSG_CHECKING([for programming environment])
   AS_CASE([$PE_ENV],
        [CRAY*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["cc"])])
                  hpx_pe_env_cppflags="‐h noomp"
                  hpx_pe_env_cflags="-h nomessage=1254"
                  hpx_pe_env_cflags_pedantic="" # LD: Can't be pedantic yet---no atomics
                  hpx_pe_env_cflags_wall=""
                  hpx_pe_env_ccasflags="-h gnu -h nomessage=1254" # ADK: We need this for ASM labels.
                  hpx_pe_env="CRAY"],
         [PGI*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["pgcc"])])
                  hpx_pe_env="PGI"],
       [INTEL*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["icc"])])
                  hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env="INTEL"],
         [GNU*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["gcc"])])
# When -pedantic is enabled in gcc, it expects that 1 or more
# parameters are passed to the variadic argument. This is
# self-defeating with use of the ,## GCC extension, so we have
# temporarily disabled -pedantic for gcc.
#                 hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_pedantic=""
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_libs="$PE_ENV_LIBS"
                  hpx_pe_env="GNU"],
       [CLANG*], [AS_IF([test -z "$CC"], [AC_SUBST([CC],["clang"])])
                  hpx_pe_env_cflags="-Wno-gnu-zero-variadic-macro-arguments"
                  hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_libs="$PE_ENV_LIBS"
                  hpx_pe_env="CLANG"],
                 [hpx_pe_env_cflags_pedantic="-pedantic"
                  hpx_pe_env_cflags_wall="-Wall"
                  hpx_pe_env_libs="$PE_ENV_LIBS"
                  hpx_pe_env="GNU"])
   AC_MSG_RESULT([$hpx_pe_env])
   AC_SUBST([PE_ENV_LIBS])])
