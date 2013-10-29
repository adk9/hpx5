# -----------------------------------------------------------------------------
# HPX_FANCY_WITH([WITH],[DEFAULT(=yes/no)],[PREFIX])
# -----------------------------------------------------------------------------
#  This macro provides a wrapper to a couple of --with options for the user to
#  configure HPX. It wraps functionality that lets the user specify the
#  external dependency in one of three ways.
#
#  1) --with-WITH=yes, --with-WITH-include=no, --with-WITH-lib=no
#
#    The package doesn't require any special CPPFLAGS, CFLAGS, LIBS, so we'll
#    just enable the defines and the conditionals that come with WITH, and set
#    the PREFIX_CFLAGS, PREFIX_LIBS, and PREFIX_PKG substitutions.
#
#  2) --with-WITH=pkg, --with-WITH-include=N/A, --with-WITH-lib=N/A
#
#    There is a pkg-config "pkg.pc" file installed somewhere in the
#    PKG_CONFIG_PATH that we want to use. This will set the PREFIX_
#    substitutions based on the PKG_CHECK_MODULES output, and set the HAVE_
#    prefixes.
#
#  3) --with-WITH={path,yes}, --with-WITH-include=incl, --with-WITH-lib=lib
#
#    This sets the PREFIX_ substitutions to the specific path provided in the
#    {include,lib} parameter, or {path/include,path/lib}.
#
# -----------------------------------------------------------------------------
# Sets
#   $with_WITH
#   $with_WITH_include
#   $with_WITH_lib
#
# Substitutes
#   PREFIX_CFLAGS
#   PREFIX_LIBS
#   PREFIX_PKG
#
# Defines
#   HAVE_PREFIX
#
# AM Conditionals
#   HAVE_PREFIX
# -----------------------------------------------------------------------------
AC_DEFUN([HPX_FANCY_WITH],[
 # 1) the main --with-WITH option
 AC_ARG_WITH([$1],
    [AS_HELP_STRING([--with-$1[[=PKGS]]], [build with $3 @<:@default=$2@:>@])],
      [], [with_$1=$2])

  # 2) the main --with-WITH-include=path option, must not be yes, if it is set,
  #    "force" --with-WITH != no
  AC_ARG_WITH([$1_include],
    [AS_HELP_STRING([--with-$1-include=dir], [include path for $3])],
      [], [with_$1_include=no])
  AS_IF([test "x$with_$1_include" = xyes],
    [AC_MSG_ERROR([--with-$1-include requires a path argument, use --with-$1[[=yes]] if the $3 include path is in your path])])
  AS_IF([test "x$with_$1" = xno -a "x$with_$1_include" != xno], [with_$1=yes])

  # 3) the main --with-WITH-lib=path option, must not be yes, if it is set,
  #    "force" --with-WITH != no
  AC_ARG_WITH([$1_lib],
    [AS_HELP_STRING([--with-$1-lib=dir], [library path for $3])],
      [], [with_$1_lib=no])
  AS_IF([test "x$with_$1_lib" = xyes],
    [AC_MSG_ERROR([--with-$1-lib requires a path argument, use --with-$1[[=yes]] if the $3 library path is in your path])])
  AS_IF([test "x$with_$1" = xno -a "x$with_$1_lib" != xno], [with_$1=yes])

  # 4) determine the correct Substitutions, based on the algorithm described
  #    above in the file comment
  AS_IF([test "x$with_$1" != xno],
    [AS_IF([test "x$with_$1" != xyes],
         [PKG_CHECK_MODULES([$3], [$with_$1])
          AC_SUBST($3_CFLAGS)
          AC_SUBST($3_LIBS)
          AC_SUBST($3_PKG, [$with_$1])
          AS_IF([test "x$with_$1_include" != xno -o "x$with_$1_lib" != xno],
              [AC_MSG_WARN([--with-$1=$with_$1 overrides --with-$1-include and --with-$1-lib])])],
         [AS_IF([test "x$with_$1_include" != xno],
              [AC_SUBST($3_CFLAGS, ["-I$with_$1_include"])])
          AS_IF([test "x$with_$1_lib" != xno],
              [AC_SUBST($3_LIBS, ["-L$with_$1_lib -l$1"])])])])

  # 5) provide the one HAVE_PREFIX define
  AS_IF([test "x$with_$1" != xno],
    AC_DEFINE([HAVE_$3], [1], [Enable $3-dependent code]))

  # 6) and the one HAVE_PREFIX automake conditional
  AM_CONDITIONAL([HAVE_$3], [test "x$with_$1" != xno])])
