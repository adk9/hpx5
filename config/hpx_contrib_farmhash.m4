# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_FARMHASH([path],[prefix])
# ------------------------------------------------------------------------------
# Sets
# Substitutes
# ------------------------------------------------------------------------------
AC_ARG_VAR([HPX_FARMHASH_CARGS], [Additional args passed to farmhash contrib])

AC_DEFUN([HPX_CONTRIB_FARMHASH],
  [$2farmhash_cargs=" $HPX_FARMHASH_CARGS"
   ACX_CONFIGURE_DIR([$1], [$1], [$$2farmhash_cargs])])
