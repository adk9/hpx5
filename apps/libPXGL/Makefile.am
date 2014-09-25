SUBDIRS              = . examples

noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

libpxgl_la_SOURCES   = src/adjacency_list.c \
                       src/edge_list.c \
                       src/sssp.c

libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la

if HAVE_PHOTON
libpxgl_la_CFLAGS    = @PHOTON_CFLAGS@
libpxgl_la_LIBADD    += -lphoton 
endif
