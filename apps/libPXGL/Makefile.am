noinst_appsdir       = $(datarootdir)/apps/

noinst_LTLIBRARIES   = libpxgl.la

libpxgl_la_SOURCES   = src/adjacency_list.c \
		       src/edge_list.c \
                       src/sssp.c

libpxgl_la_CPPFLAGS  = -I$(top_srcdir)/include -I./include
libpxgl_la_LIBADD    = $(top_builddir)/libhpx/libhpx.la

noinst_apps_PROGRAMS = sssp

sssp_SOURCES   = examples/sssp.c
sssp_LDFLAGS   = -static

sssp_CPPFLAGS  = -I$(top_srcdir)/include -I./include
sssp_CFLAGS    = @PHOTON_CFLAGS@ @MPI_CFLAGS@
sssp_CFLAGS   += @HWLOC_CFLAGS@ @PAPI_CFLAGS@
sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la $(top_builddir)/libhpx/libhpx.la

if HAVE_PHOTON
libpxgl_la_LIBADD   += @PHOTON_LIBS@ @MPI_LIBS@
sssp_LDADD    += @PHOTON_LIBS@ @MPI_LIBS@
endif
