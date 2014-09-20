# -*- autoconf -*---------------------------------------------------------------
# HPX_JEMALOC()
# ------------------------------------------------------------------------------
# Make sure that a compatible installation of jemalloc can be found.
#
# Sets
#   hpx_with_jemalloc_include_dir
#   hpx_with_jemalloc_lib_dir
#   hpx_with_jemalloc_lib
#   hpx_have_jemalloc
#
# Substitutes
#   HPX_JEMALLOC_CPPFLAGS
#   HPX_JEMALLOC_LDFLAGS
#   HPX_JEMALLOC_LIBS
#
# Defines
#   HPX_HAVE_JEMALLOC_H
#
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_JEMALLOC],
 [AC_MSG_NOTICE([cheking for jemalloc with support for custom chunk allocation])
  
  # Make sure we're using C and checkpoint flags that we might overwrite.
  AC_LANG_PUSH(C)
  hpx_old_jemalloc_CPPFLAGS="$CPPFLAGS"
  hpx_old_jemalloc_LDFLAGS="$LDFLAGS"
  hpx_old_jemalloc_LIBS="$LIBS"

  # Allow the user to set explicit paths to jemalloc.
  AC_ARG_WITH([jemalloc-include-dir],
   [AS_HELP_STRING([--with-jemalloc-include-dir], [path to <jemalloc/jemalloc.h>])],
   [hpx_with_jemalloc_include_dir=$withval],
   [hpx_with_jemalloc_include_dir=no]
  )
  
  AS_IF([test "x$hpx_with_jemalloc_include_dir" != xno],
   [AC_SUBST(HPX_JEMALLOC_CPPFLAGS, ["-I$hpx_with_jemalloc_include_dir"])]
  )
   
  AC_ARG_WITH([jemalloc-lib-dir],
   [AS_HELP_STRING([--with-jemalloc-lib-dir], [path to libjemalloc.{a,so}])],
   [hpx_with_jemalloc_lib_dir=$withval],
   [hpx_with_jemalloc_lib_dir=no]
  )

  AS_IF([test "x$hpx_with_jemalloc_lib_dir" != xno],
   [AC_SUBST(HPX_JEMALLOC_LDFLAGS,
     ["-L$hpx_with_jemalloc_lib_dir -Wl,-rpath,$hpx_with_jemalloc_lib_dir"])
   ]
  )

  AC_ARG_WITH([jemalloc-lib],
   [AS_HELP_STRING([--with-jemalloc-lib], [jemalloc library @<:@default=jemalloc@:>@])],
   [hpx_with_jemalloc_lib=$withval],
   [hpx_with_jemalloc_lib=jemalloc]
  )

  AC_SUBST(HPX_JEMALLOC_LIBS, [-l"$hpx_with_jemalloc_lib"])
  
  # Check for the jemalloc header, using the passed include directory if used.
  CPPFLAGS="$CPPFLAGS $HPX_JEMALLOC_CPPFLAGS"
  AC_CHECK_HEADER([jemalloc/jemalloc.h],
   [hpx_have_jemalloc=yes
    AC_DEFINE(HPX_HAVE_JEMALLOC_H, [1], [Found the jemalloc.h header])
   ],
   [hpx_have_jemalloc=no
    AC_MSG_WARN([Could not find jemalloc])
   ],
   [#include <stddef.h>
    #include <stdbool.h>
   ]
  )

  # If we found the jemalloc header, check for the library using the passed
  # library directory if used.
  AS_IF([test "x$hpx_have_jemalloc" != xno],
   [LDFLAGS="$LDFLAGS $HPX_JEMALLOC_LDFLAGS"
    AC_CHECK_LIB([$hpx_with_jemalloc_lib], [mallctl],
     [hpx_have_jemalloc=yes],
     [hpx_have_jemalloc=no
      AC_MSG_WARN([Could not find libjemalloc.{a,so}])
     ]
    )
   ]
  )

  # If we found both the header and the library, test to make sure that the
  # library supports the "arena.<i>.chunk.alloc" custom allocator. This won't
  # work during cross compilation, where we just assume we have the correct
  # version---we test at runtime anyway, this is just a way of detecting version
  # mismatches early if possible.
  AS_IF([test "x$hpx_have_jemalloc" != xno],
   [AC_MSG_CHECKING([jemalloc support for custom chunk allocation])
    LIBS="$LIBS $HPX_JEMALLOC_LIBS"
    AC_RUN_IFELSE(
     [AC_LANG_PROGRAM(
        [[#include <stddef.h>
          #include <stdbool.h>
          #include <jemalloc/jemalloc.h>
        ]],
        [[return mallctl("arena.0.chunk.alloc", NULL, NULL, NULL, 0);]]
      )
     ],
     [hpx_have_jemalloc=yes
      AC_MSG_RESULT([yes])
     ],
     [hpx_have_jemalloc=no
      AC_MSG_RESULT([no])
     ],
     [hpx_have_jemalloc=yes
      AC_MSG_RESULT([cross-compilation defaults to yes])
     ]
    )
   ]
  )

  # restore flags and outer language
  LIBS="$hpx_old_jemalloc_LIBS"
  LDFLAGS="$hpx_old_jemalloc_LDFLAGS"
  CPPFLAGS="$hpx_old_jemalloc_CPPFLAGS"
  AC_LANG_POP

  AS_IF([test "x$hpx_have_jemalloc" != xno],
   [AC_MSG_RESULT([jemalloc success])],
   [AC_MSG_ERROR([
   
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
HPX ERROR

jemalloc could not be configured correctly. HPX depends on functionality present
in the jemalloc development branch that can be downloaded from
https://github.com/jemalloc/jemalloc. Please ensure that your version of
jemalloc is up to date, and that it's paths are can be found in the standard
search paths on your system. 

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

])])])
