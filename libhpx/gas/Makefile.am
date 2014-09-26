# LT Utility libraries
noinst_LTLIBRARIES	= libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libhpx/network/
libgas_la_SOURCES  = addr.c btt.c bitmap_alloc.c jemalloc_wrappers.c \
                     agas_btt.c agas_switch_btt.c \
                     pgas.c pgas_heap.c pgas_btt.c \
                     local_only_btt.c \
                     parcel_memget.c parcel_memput.c parcel_memcpy.c 
