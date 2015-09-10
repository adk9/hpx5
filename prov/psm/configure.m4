dnl Configury specific to the libfabric PSM provider

dnl Called to configure this provider
dnl
dnl Arguments:
dnl
dnl $1: action if configured successfully
dnl $2: action if not configured successfully
dnl
AC_DEFUN([FI_PSM_CONFIGURE],[
	# Determine if we can support the psm provider
	psm_happy=0
	AS_IF([test x"$enable_psm" != x"no"],
	      [FI_CHECK_PACKAGE([psm],
				[psm.h],
				[psm_infinipath],
				[psm_init],
				[],
				[],
				[],
				[psm_happy=1],
				[psm_happy=0])
	       AS_IF([test $psm_happy -eq 1],
		     [AC_MSG_CHECKING([if PSM version is 1.x])
		      AC_RUN_IFELSE([AC_LANG_SOURCE(
						    [[
						    #include <psm.h>
						    int main()
						    {
							return PSM_VERNO_MAJOR < 2 ? 0 : 1;
						    }
						    ]]
						   )],
				    [AC_MSG_RESULT([yes])],
				    [AC_MSG_RESULT([no]); psm_happy=0])
		     ])
	       AS_IF([test $psm_happy -eq 1],
		     [AC_CHECK_TYPE([psm_epconn_t],
		                    [],
				    [psm_happy=0],
				    [[#include <psm.h>]])])
	      ])

	AS_IF([test $psm_happy -eq 1], [$1], [$2])

	psm_CPPFLAGS="$CPPFLAGS $psm_CPPFLAGS"
	psm_LDFLAGS="$LDFLAGS $psm_LDFLAGS"
	psm_LIBS="$LIBS $psm_LIBS"
	CPPFLAGS="$psm_orig_CPPFLAGS"
	LDFLAGS="$psm_orig_LDFLAGS"
])
