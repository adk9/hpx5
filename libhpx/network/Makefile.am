# The networking library
noinst_LTLIBRARIES	= libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/contrib
libnetwork_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libnetwork_la_SOURCES  = locality.c network.c allocator.c block.c cache.c parcel.c heavy.c
libnetwork_la_LDFLAGS  = -pthread