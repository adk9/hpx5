SUBDIRS              = . include examples

noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

libpxgl_la_SOURCES   = src/adjacency_list.c \
                       src/edge_list.c \
                       src/sssp_common.c \
                       src/sssp_chaotic.c \
                       src/sssp_dc.c \
                       src/sssp_delta.c \
		       src/dimacs.c \
		       src/gteps.c \
		       src/statistics.c \
                       src/pqueue.c \
                       src/pqueue_wrapper.c \
                       src/pqueue.c \
                       src/termination.c

libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include -DSSSP_64BIT
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la -lm
