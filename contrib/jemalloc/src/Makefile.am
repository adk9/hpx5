noinst_LTLIBRARIES	= libhpx_jemalloc.la 

if ENABLE_JEMALLOC
noinst_LTLIBRARIES      += libjemalloc.la
endif

libhpx_jemalloc_la_SOURCES  = arena.c atomic.c base.c bitmap.c chunk.c chunk_dss.c chunk_mmap.c \
                              ckh.c ctl.c extent.c hash.c huge.c jemalloc.c mb.c mutex.c prof.c \
                              quarantine.c rtree.c stats.c tcache.c tsd.c util.c

libhpx_jemalloc_la_CPPFLAGS = -I$(top_srcdir)/contrib/jemalloc/include $(HPX_JEMALLOC_BUILD_CPPFLAGS) -DJEMALLOC_PREFIX=\"libhpx_global_\" -DJEMALLOC_CPREFIX=\"LIBHPX_GLOBAL_\" -DJEMALLOC_RENAME=libhpx_global_ -DJEMALLOC_NAMESPACE=hpx
libhpx_jemalloc_la_LIBADD = $(HPX_JEMALLOC_BUILD_LIBS)

if ENABLE_JEMALLOC
libjemalloc_la_SOURCES  = arena.c atomic.c base.c bitmap.c chunk.c chunk_dss.c chunk_mmap.c \
                          ckh.c ctl.c extent.c hash.c huge.c jemalloc.c mb.c mutex.c prof.c \
                          quarantine.c rtree.c stats.c tcache.c tsd.c util.c

libjemalloc_la_CPPFLAGS = -I$(top_srcdir)/contrib/jemalloc/include $(HPX_JEMALLOC_BUILD_CPPFLAGS) -DJEMALLOC_RENAME="" -DJEMALLOC_NAMESPACE=global
libjemalloc_la_LIBADD = $(HPX_JEMALLOC_BUILD_LIBS)
endif
