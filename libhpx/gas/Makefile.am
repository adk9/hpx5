# LT Utility libraries
noinst_LTLIBRARIES	= libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libhpx/network/
libgas_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libgas_la_SOURCES  = addr.c pgas.c
libgas_la_LIBADD   =
