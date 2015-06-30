AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@PHOTON_SRCDIR@/include        \
	    -I@PHOTON_SRCDIR@/src/contrib    \
	    -I@PHOTON_SRCDIR@/src/contrib/libsync/include

noinst_LTLIBRARIES = libutil.la
libutil_la_SOURCES = logging.c htable.c util.c avl.c
