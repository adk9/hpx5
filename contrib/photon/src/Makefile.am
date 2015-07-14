AM_CFLAGS = @CFLAGS@                            \
            -I@PHOTON_SRCDIR@/include           \
            -I@PHOTON_SRCDIR@/src/util          \
            -I@PHOTON_SRCDIR@/src/libphoton     \
            -I@PHOTON_SRCDIR@/src/forwarder     \
	    -I@PHOTON_SRCDIR@/src/contrib       \
	    -I@PHOTON_SRCDIR@/src/contrib/libsync/include
#-DPHOTON_MULTITHREADED

SUBDIRS      = util contrib
DIST_SUBDIRS = util contrib

lib_LTLIBRARIES = libphoton.la
libphoton_la_SOURCES = libphoton/libphoton.c                   \
		       libphoton/photon_request.c              \
		       libphoton/photon_event.c                \
		       libphoton/photon_pwc.c                  \
                       libphoton/photon_backend.c              \
                       libphoton/photon_exchange.c             \
                       libphoton/photon_buffer.c               \
                       libphoton/photon_msgbuffer.c            \
                       libphoton/photon_buffertable.c          \
                       libphoton/photon_rdma_ledger.c          \
                       libphoton/photon_rdma_INFO_ledger.c     \
                       libphoton/photon_rdma_EAGER_buf.c

libphoton_ladir = @PHOTON_SRCDIR@/src/libphoton
libphoton_la_LDFLAGS = @THREAD_LIBS@
libphoton_la_LIBADD = @PHOTON_BUILDDIR@/src/util/libutil.la    \
 @PHOTON_BUILDDIR@/src/contrib/bit_array/libbit_array.la       \
 @PHOTON_BUILDDIR@/src/contrib/libsync/libsync.la

if HAVE_VERBS
AM_CFLAGS += -I@PHOTON_SRCDIR@/src/verbs
libphoton_la_LDFLAGS += @VERBS_LIBS@
libphoton_la_SOURCES += verbs/verbs.c                          \
                    verbs/verbs_util.c                         \
                    verbs/verbs_connect.c                      \
                    verbs/verbs_buffer.c                       \
                    verbs/verbs_ud.c
endif

if HAVE_UGNI
AM_CFLAGS += -I@PHOTON_SRCDIR@/src/ugni @UGNI_CFLAGS@
libphoton_la_LDFLAGS += @UGNI_LIBS@
libphoton_la_SOURCES += ugni/photon_ugni.c                     \
                    ugni/photon_ugni_connect.c                 \
                    ugni/photon_ugni_buffer.c                  \
                    ugni/utility_functions.c

#noinst_bindir = @PHOTON_BUILDDIR@/test
#noinst_bin_PROGRAMS = ugni_test
#ugni_test_CFLAGS = ${AM_CFLAGS} @UGNI_CFLAGS@ @MPI_FLAGS@
#ugni_test_LDFLAGS = @UGNI_LIBS@ @MPI_LIBS@
#ugni_test_SOURCES = ugni/utility_functions.c
endif

if HAVE_LIBFABRIC
AM_CFLAGS += -I@PHOTON_SRCDIR@/src/fi @PHOTON_LIBFABRIC_CPPFLAGS@
libphoton_la_SOURCES += fi/photon_fi.c
libphoton_la_LIBADD += @PHOTON_BUILDDIR@/src/contrib/libfabric/src/libfabric.la
endif

if HAVE_MPI
AM_CFLAGS += @MPI_CFLAGS@
libphoton_la_LDFLAGS += @MPI_LIBS@
endif

if HAVE_PMI
AM_CFLAGS += @PMI_CFLAGS@
libphoton_la_LDFLAGS += @PMI_LIBS@
endif

if HAVE_XSP
AM_CFLAGS += @XSP_CFLAGS@
libphoton_la_LDFLAGS += @XSP_LIBS@
libphoton_la_SOURCES += forwarder/photon_xsp_forwarder.c
endif
