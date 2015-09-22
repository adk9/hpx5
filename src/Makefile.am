SUBDIRS       = util contrib
DIST_SUBDIRS  = util contrib

if HAVE_VERBS
SUBDIRS      += verbs
DIST_SUBDIRS += verbs
endif

if HAVE_UGNI
SUBDIRS      += ugni
DIST_SUBDIRS += ugni
endif

if HAVE_LIBFABRIC
SUBDIRS      += fi
DIST_SUBDIRS += fi
endif

if HAVE_XSP
if BUILD_FORWARDER
SUBDIRS      += forwarder
DIST_SUBDIRS += forwarder
endif
endif

lib_LTLIBRARIES       = libphoton.la
libphoton_la_SOURCES  = libphoton/libphoton.c                   \
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

libphoton_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBPHOTON_CPPFLAGS)
libphoton_la_CFLAGS   = $(LIBPHOTON_CFLAGS)                     \
			-I$(top_srcdir)/src/verbs               \
			-I$(top_srcdir)/src/ugni                \
			-I$(top_srcdir)/src/fi                  \
			-I$(top_srcdir)/src/forwarder
libphoton_la_CXXFLAGS = $(LIBPHOTON_CXXFLAGS)

libphoton_ladir       = $(top_builddir)/src/libphoton
libphoton_la_LDFLAGS  = $(LIBPHOTON_LDFLAGS)
libphoton_la_LIBADD   = $(LIBPHOTON_LIBS) $(LIBPHOTON_LIBADD)                    \
			$(top_builddir)/src/util/libutil.la                      \
		        $(top_builddir)/src/contrib/bit_array/libbit_array.la    \
		        $(top_builddir)/src/contrib/libsync/libsync.la
