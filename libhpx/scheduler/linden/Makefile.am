SUBDIRS = gc

# The linden priority queue.
noinst_LTLIBRARIES       = liblinden.la
noinst_HEADERS           = prioq.h common.h

liblinden_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS) -DINTEL -DCACHE_LINE_SIZE=@HPX_CACHELINE_SIZE@
liblinden_la_CFLAGS   = $(LIBHPX_CFLAGS)
liblinden_la_SOURCES  = common.c perf_meas.c prioq.c 
liblinden_la_LIBADD   = gc/libgc.la
