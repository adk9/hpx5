AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@PHOTON_SRCDIR@/include

noinst_LTLIBRARIES = libutil.la
libutil_la_SOURCES = logging.c htable.c util.c 

if ENABLE_PHOTON_DEBUG
AM_CFLAGS += -DDEBUG
endif

if ENABLE_PHOTON_TRACE
AM_CFLAGS += -DCALLTRACE
endif
