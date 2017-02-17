# The gc priority queue.
noinst_LTLIBRARIES       = libgc.la
noinst_HEADERS           = gc.h intel_defns.h portable_defns.h ptst.h random.h

libgc_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS) -DINTEL -DCACHE_LINE_SIZE=@HPX_CACHELINE_SIZE@
libgc_la_CFLAGS   = $(LIBHPX_CFLAGS)
libgc_la_SOURCES  = gc.c ptst.c
