noinst_appsdir       = $(datarootdir)/apps/

noinst_apps_PROGRAMS = sssp

sssp_SOURCES   = sssp.c

sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include -DSSSP_64BIT
sssp_CFLAGS    = @HWLOC_CFLAGS@ @PAPI_CFLAGS@
sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la
