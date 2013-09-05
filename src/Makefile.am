AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include           \
	    -I@top_srcdir@/src/util

SUBDIRS = .

lib_LTLIBRARIES = libphoton.la
libphoton_la_SOURCES = libphoton/libphoton.c           \
		       libphoton/photon_backend.c

libphoton_ladir = @top_srcdir@/src/libphoton
libphoton_la_HEADERS = 
libphoton_la_LDFLAGS = @top_srcdir@/src/util/libutil.la
libphoton_la_LIBADD = 

if HAVE_VERBS
AM_CFLAGS += -I@top_srcdir@/src/verbs                  \
	     -I@top_srcdir@/src/libphoton
libphoton_la_SOURCES += verbs/verbs.c                  \
			verbs/verbs_exchange.c         \
			verbs/verbs_connect.c          \
			verbs/verbs_cma_connect.c      \
			verbs/verbs_buffer.c           \
			verbs/verbs_remote_buffer.c    \
			verbs/verbs_rdma_FIN_ledger.c  \
			verbs/verbs_rdma_INFO_ledger.c \
			verbs/buffertable.c
endif

#if HAVE_UGNI
#libphoton_la_SOURCES +=
#endif
