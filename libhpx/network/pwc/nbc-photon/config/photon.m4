dnl 
dnl  Copyright (c) 2006 The Trustees of Indiana University and Indiana
dnl                     University Research and Technology
dnl                     Corporation.  All rights reserved.
dnl  Copyright (c) 2006 The Technical University of Chemnitz. All 
dnl                     rights reserved.
dnl 
dnl  Author(s): Torsten Hoefler <htor@cs.indiana.edu>
dnl 
AC_DEFUN([TEST_PHOTON],
    [AC_ARG_WITH(photon, AC_HELP_STRING([--with-photon], [compile with Photon support (ARG can be the path to the root Open Fabrics directory)]))
    photon_found=no
    if test x"${with_photon}" = xyes; then
        AC_CHECK_HEADER(photon.h, photon_found=yes, [AC_MSG_ERROR([OFED selected but not available!])])
    elif test x"${with_photon}" != x; then
        AC_CHECK_HEADER(${with_photon}/include/photon.h, [ng_photon_path=${with_photon}; photon_found=yes], [AC_MSG_ERROR([Can't find OFED in ${with_photon}])])
    fi
    if test x"${with_photon}" != x; then
        # try to use the library ...
        LIBS="${LIBS} -L${ng_photon_path}/lib"
        AC_CHECK_LIB([photon], [photon_register_buffer])
      
        AC_DEFINE(HAVE_PHOTON, 1, enables the photon module)
        AC_MSG_NOTICE([photon support enabled])
        if test x${ng_photon_path} != x; then
          CFLAGS="${CFLAGS} -I${ng_photon_path}/include"
        fi
        have_photon=1
    fi
    ]
)
