# The AGAS library
noinst_LTLIBRARIES  = libagas.la

libagas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBCUCKOO_CPPFLAGS)
libagas_la_SOURCES  = agas.c btt.cc chunk_table.cc string.c local.c lva.c
libagas_la_LIBADD   = $(HPX_LIBCUCKOO_LIBS)

if BUILD_JEMALLOC
libagas_la_SOURCES += jemalloc_cyclic.c  jemalloc_global.c
endif
