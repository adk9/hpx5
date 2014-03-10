# The parcel library
noinst_LTLIBRARIES	= libparcel.la

libparcel_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx -I$(srcdir)/..
libparcel_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libparcel_la_SOURCES  = parcel.c allocator.c block.c cache.c
