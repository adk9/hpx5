# The networking library
noinst_LTLIBRARIES	= libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/contrib -DJEMALLOC_RENAME=libhpx_global_
libnetwork_la_CFLAGS   = 
libnetwork_la_SOURCES  = locality.c network.c parcel.c \
                         routing/routing.c routing/dummy.c
libnetwork_la_LIBADD   =

# if HAVE_TREMA
# libnetwork_la_SOURCES  += routing/trema.c
# libnetwork_la_CFLAGS   += @TREMA_CFLAGS@
# libnetwork_la_LIBADD   += @TREMA_LIBS@
# endif

# if HAVE_FLOODLIGHT
# libnetwork_la_SOURCES  += routing/floodlight.c
# libnetwork_la_CFLAGS   += @JANSSON_CFLAGS@ @CURL_CFLAGS@
# libnetwork_la_LIBADD   += @JANSSON_LIBS@ @CURL_LIBS@
# endif
