SUBDIRS       = util contrib
DIST_SUBDIRS  = util contrib verbs ugni fi forwarder

if HAVE_VERBS
SUBDIRS      += verbs
endif

if HAVE_UGNI
SUBDIRS      += ugni
endif

if HAVE_LIBFABRIC
SUBDIRS      += fi
endif

if HAVE_XSP
if BUILD_FORWARDER
SUBDIRS      += forwarder
endif
endif

lib_LTLIBRARIES       = libphoton.la
libphoton_la_SOURCES  = libphoton/libphoton.h                   \
			libphoton/libphoton.c                   \
		        libphoton/photon_request.h              \
		        libphoton/photon_request.c              \
		        libphoton/photon_event.h                \
		        libphoton/photon_event.c                \
		        libphoton/photon_pwc.h                  \
		        libphoton/photon_pwc.c                  \
                        libphoton/photon_backend.h              \
                        libphoton/photon_backend.c              \
                        libphoton/photon_exchange.h             \
                        libphoton/photon_exchange.c             \
                        libphoton/photon_buffer.h               \
                        libphoton/photon_buffer.c               \
                        libphoton/photon_msgbuffer.h            \
                        libphoton/photon_msgbuffer.c            \
                        libphoton/photon_buffertable.h          \
                        libphoton/photon_buffertable.c          \
                        libphoton/photon_rdma_ledger.h          \
                        libphoton/photon_rdma_ledger.c          \
                        libphoton/photon_rdma_INFO_ledger.h     \
                        libphoton/photon_rdma_INFO_ledger.c     \
                        libphoton/photon_rdma_EAGER_buf.h       \
                        libphoton/photon_rdma_EAGER_buf.c

libphoton_la_CPPFLAGS = $(LIBPHOTON_CPPFLAGS)
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
