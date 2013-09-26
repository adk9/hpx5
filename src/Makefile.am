AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include           \
	    -I@top_srcdir@/src/util          \
	    -I@top_srcdir@/src/libphoton
#-DPHOTON_MULTITHREADED
SUBDIRS = .

lib_LTLIBRARIES = libphoton.la
libphoton_la_SOURCES = libphoton/libphoton.c           \
		       libphoton/photon_backend.c              \
		       libphoton/photon_buffer.c               \
		       libphoton/photon_buffertable.c          \
		       libphoton/photon_rdma_FIN_ledger.c      \
		       libphoton/photon_rdma_INFO_ledger.c

libphoton_ladir = @top_srcdir@/src/libphoton
libphoton_la_HEADERS = 
libphoton_la_LDFLAGS = @THREAD_LIBS@ @top_srcdir@/src/util/libutil.la 
libphoton_la_LIBADD = 

if HAVE_VERBS
AM_CFLAGS += -I@top_srcdir@/src/verbs
libphoton_la_LDFLAGS += @VERBS_LIBS@
libphoton_la_SOURCES += verbs/verbs.c                  \
		    verbs/verbs_exchange.c                     \
		    verbs/verbs_connect.c                      \
		    verbs/verbs_buffer.c
endif

if HAVE_UGNI
AM_CFLAGS += -I@top_srcdir@/src/ugni @UGNI_CFLAGS@
libphoton_la_LDFLAGS += @UGNI_LIBS@
libphoton_la_SOURCES += ugni/photon_ugni.c             \
		    ugni/photon_ugni_buffer.c
endif
