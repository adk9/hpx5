noinst_appsdir       = $(datarootdir)/apps/

noinst_apps_PROGRAMS = sssp

sssp_SOURCES   = sssp.c

sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include
sssp_CFLAGS    = @HWLOC_CFLAGS@ @PAPI_CFLAGS@
sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la

if HAVE_PHOTON
sssp_CPPFLAGS += $(HPX_JEMALLOC_CPPFLAGS)
sssp_CFLAGS   += @PHOTON_CFLAGS@
sssp_LDFLAGS   = $(HPX_JEMALLOC_LDFLAGS)
endif
