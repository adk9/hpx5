# The networking library
noinst_LTLIBRARIES	= libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/contrib
libnetwork_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libnetwork_la_SOURCES  = locality.c network.c allocator.c block.c cache.c parcel.c heavy.c\
                         routing/routing.c
libnetwork_la_LDFLAGS  = -pthread
libnetwork_la_LIBADD   =

if HAVE_TREMA
libnetwork_la_SOURCES  += routing/trema.c
libnetwork_la_CFLAGS   += @TREMA_CFLAGS@
libnetwork_la_LIBADD   += @TREMA_LIBS@
endif

if HAVE_FLOODLIGHT
libnetwork_la_SOURCES  += routing/floodlight.c
libnetwork_la_CFLAGS   += @JANSSON_CFLAGS@ @CURL_CFLAGS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@ # photon and mpi because of bravo_ids.h
libnetwork_la_LIBADD   += @JANSSON_LIBS@ @CURL_LIBS@
endif
