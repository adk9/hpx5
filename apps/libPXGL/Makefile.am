SUBDIRS              = . include examples

noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

CC=tau_cc.sh

libpxgl_la_SOURCES   = src/adjacency_list.c \
                       src/edge_list.c \
                       src/sssp.c \
		       src/dimacs.c \
		       src/gteps.c \
		       src/statistics.c		

libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include -DSSSP_64BIT
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la -lm
