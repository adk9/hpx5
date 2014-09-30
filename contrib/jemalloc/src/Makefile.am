lib_LTLIBRARIES	= libhpx_jemalloc.la

libhpx_jemalloc_la_SOURCES  = arena.c atomic.c base.c bitmap.c chunk.c chunk_dss.c chunk_mmap.c \
                              ckh.c ctl.c extent.c hash.c huge.c jemalloc.c mb.c mutex.c prof.c \
                              quarantine.c rtree.c stats.c tcache.c tsd.c util.c

libhpx_jemalloc_la_CPPFLAGS = -I$(top_srcdir)/contrib/jemalloc/include
