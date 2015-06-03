# LT Utility libraries
noinst_LTLIBRARIES	  = libmemory.la

# libboot files and flags
libmemory_la_CPPFLAGS = -I$(top_srcdir)/include
libmemory_la_SOURCES  = glue.c

if HAVE_NETWORK
libmemory_la_SOURCES += jemalloc.c
endif
