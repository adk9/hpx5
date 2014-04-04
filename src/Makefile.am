AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include           \
	    -I@top_srcdir@/src/util          \
	    -I@top_srcdir@/src/libphoton     \
	    -I@top_srcdir@/src/forwarder
#-DPHOTON_MULTITHREADED
SUBDIRS = .

lib_LTLIBRARIES = libphoton.la
libphoton_la_SOURCES = libphoton/libphoton.c           \
		       libphoton/photon_backend.c              \
		       libphoton/photon_exchange.c             \
		       libphoton/photon_buffer.c               \
		       libphoton/photon_buffertable.c          \
		       libphoton/photon_rdma_FIN_ledger.c      \
		       libphoton/photon_rdma_INFO_ledger.c

libphoton_ladir = @top_srcdir@/src/libphoton
libphoton_la_LDFLAGS = @THREAD_LIBS@ @MPI_LIBS@
libphoton_la_LIBADD = @top_srcdir@/src/util/libutil.la

if HAVE_VERBS
AM_CFLAGS += -I@top_srcdir@/src/verbs
libphoton_la_LDFLAGS += @VERBS_LIBS@
libphoton_la_SOURCES += verbs/verbs.c                  \
		    verbs/verbs_exchange.c                     \
		    verbs/verbs_connect.c                      \
		    verbs/verbs_buffer.c                       \
	            verbs/verbs_ud.c
endif

if HAVE_UGNI
AM_CFLAGS += -I@top_srcdir@/src/ugni @UGNI_CFLAGS@
libphoton_la_LDFLAGS += @UGNI_LIBS@
libphoton_la_SOURCES += ugni/photon_ugni.c             \
		    ugni/photon_ugni_exchange.c                \
		    ugni/photon_ugni_connect.c                 \
		    ugni/photon_ugni_buffer.c                  \
		    ugni/utility_functions.c

noinst_bindir = @abs_top_builddir@/test
noinst_bin_PROGRAMS = ugni_test
ugni_test_CFLAGS = ${AM_CFLAGS} @UGNI_CFLAGS@
ugni_test_LDFLAGS = @MPI_LIBS@ @UGNI_LIBS@
ugni_test_SOURCES = ugni/ugni_test.c ugni/utility_functions.c
endif

if HAVE_XSP
AM_CFLAGS += @XSP_CFLAGS@
libphoton_la_LDFLAGS += @XSP_LIBS@
libphoton_la_SOURCES += forwarder/photon_xsp_forwarder.c
endif
