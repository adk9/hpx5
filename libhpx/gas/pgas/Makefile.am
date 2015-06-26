# The PGAS library
noinst_LTLIBRARIES  = libpgas.la

libpgas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBCUCKOO_CPPFLAGS)
libpgas_la_SOURCES  = actions.c \
                      pgas.c \
                      heap.c

if BUILD_JEMALLOC
libpgas_la_SOURCES += jemalloc_global.c jemalloc_cyclic.c
endif
