appsdir       = $(datarootdir)/apps/

apps_PROGRAMS = sssp

sssp_SOURCES   = sssp.c

if CPU_ARM
sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include -DSSSP_32BIT
else
sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include -DSSSP_64BIT
endif
sssp_CFLAGS    = 
sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la $(HPX_LDADD)
sssp_DEPENDENCIES = $(HPX_DEPS)

if HAVE_MPI
sssp_CFLAGS           += @MPI_CFLAGS@
endif
