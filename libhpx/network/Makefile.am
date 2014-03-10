SUBDIRS = transport

# The networking library
noinst_LTLIBRARIES	= libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx -I$(srcdir)/..
libnetwork_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libnetwork_la_SOURCES  = network.c addr.c
libnetwork_la_LIBADD   = transport/libtransport.la
