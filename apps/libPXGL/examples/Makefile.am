noinst_appsdir       = $(datarootdir)/apps/

noinst_apps_PROGRAMS = sssp

sssp_SOURCES   = sssp.c

CC = tau_cc.sh

sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include
sssp_CPPFLAGS  += -I/u/jayaajay/tau-2.23.2b3/include

sssp_CFLAGS    = @HWLOC_CFLAGS@ @PAPI_CFLAGS@
sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la
