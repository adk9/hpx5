# LT Utility libraries
noinst_LTLIBRARIES	  = libmemory.la

# libboot files and flags
libmemory_la_CPPFLAGS = -I$(top_srcdir)/include
libmemory_la_SOURCES  = default.c glue.c common.c

if ENABLE_JEMALLOC
libmemory_la_SOURCES += jemalloc.c
endif

if BUILD_JEMALLOC_GLOBAL
libmemory_la_SOURCES += global.c
endif

if BUILD_JEMALLOC_REGISTERED
libmemory_la_SOURCES += registered.c
endif
