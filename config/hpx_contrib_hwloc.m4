# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_HWLOC([path],[prefix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_HWLOC],
  [HWLOC_SET_SYMBOL_PREFIX([$2])
   # Disable features that are not required for our embedded build
   enable_libxml2=no
   enable_libnuma=no
   enable_pci=no
   enable_opencl=no
   enable_cuda=no
   enable_nvml=no
   enable_gl=no
   HWLOC_SETUP_CORE([$1], [have_hwloc=yes], [have_hwloc=no])
   unset enable_libxml2
   unset enable_libnuma
   unset enable_pci
   unset enable_opencl
   unset enable_cuda
   unset enable_nvml
   unset enable_gl
   AS_IF([test "x$have_hwloc" != xyes],
     [AC_MSG_ERROR([could not configure hwloc])],
     [HWLOC_DO_AM_CONDITIONALS
      $2hwloc_cppflags="\$(HWLOC_EMBEDDED_CPPFLAGS)"
      $2hwloc_cflags="\$(HWLOC_EMBEDDED_CFLAGS)"
      $2hwloc_libs="\$(HWLOC_EMBEDDED_LIBS)"])])
