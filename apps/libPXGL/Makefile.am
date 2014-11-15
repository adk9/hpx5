SUBDIRS              = . examples

noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

nodist_include_HEADERS = include/adjacency_list.h \
                         include/dimacs.h \
                         include/edge_list.h \
                         include/libpxgl.h \
                         include/sssp.h \
			 include/defs.h \
	                 include/gteps.h \
			 include/statistics.h		

libpxgl_la_SOURCES   = src/adjacency_list.c \
                       src/edge_list.c \
                       src/sssp.c \
		       src/dimacs.c \
		       src/gteps.c \
		       src/statistics.c		

libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include -DSSSP_64BIT
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la -lm
