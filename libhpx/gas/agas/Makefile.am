# The AGAS library
noinst_LTLIBRARIES  = libagas.la

libagas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBCUCKOO_CPPFLAGS)
libagas_la_SOURCES  = agas.c btt.cc chunk_table.cc
libagas_la_LIBADD   = $(HPX_LIBCUCKOO_LIBS)
