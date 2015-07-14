# -*- autoconf -*---------------------------------------------------------------
# HPX_CHECK_CMPXCHG16B
#
# Check if architecture supports CMPXCHG16B instruction.
#
# Defines
#   HAVE_CMPXCHG16B
# ------------------------------------------------------------------------------

AC_DEFUN([HPX_CHECK_CMPXCHG16B], [
 AC_MSG_CHECKING([for support of CMPXCHG16B instruction])
 AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
    [[ __asm__ __volatile__("lock cmpxchg16b (%rdi)") ]])],
    [AC_MSG_RESULT([yes])]
    have_cmpxchg16b=yes,
    [AC_MSG_RESULT([no])])
])
