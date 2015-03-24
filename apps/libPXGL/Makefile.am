SUBDIRS              = . include examples generatorhelper/include

noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

libpxgl_la_SOURCES   = src/adjacency_list.c \
                       src/edge_list.c \
                       src/sssp_common.c \
                       src/sssp_chaotic.c \
                       src/sssp_dc.c \
                       src/sssp_dc1.c \
                       src/sssp_delta.c \
		       src/sssp_kla.c \
		       src/dimacs.c \
		       src/gteps.c \
		       src/statistics.c \
                       src/pqueue.c \
                       src/termination.c \
		       src/graph500_edge_list_generator.c \
                       src/graph_generator.c \
		       src/bfs.c \
                       generatorhelper/src/globals.c \
                       generatorhelper/src/prng.c  \
                       generatorhelper/src/splittable_mrg.c
                       #generatorhelper/src/mrg_transitions.c

if CPU_ARM
libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include -DPXGL_32BIT
else
libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/include -DPXGL_64BIT 
endif
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la $(top_builddir)/libsync/libsync.la $(top_builddir)/libsync/arch/libarch.la -lm
