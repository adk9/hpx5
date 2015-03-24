appsdir       = $(datarootdir)/apps/

apps_PROGRAMS = sssp

sssp_SOURCES   = sssp.c 

AM_LDFLAGS     = $(HPX_JEMALLOC_AS_MALLOC_RPATH) $(HPX_JEMALLOC_RPATH)

if CPU_ARM
sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include -DPXGL_32BIT
else
sssp_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../include -DPXGL_64BIT
endif
sssp_CFLAGS    = 

sssp_LDADD     = $(top_builddir)/apps/libPXGL/libpxgl.la $(top_builddir)/libsync/libsync.la $(top_builddir)/libsync/arch/libarch.la $(HPX_LIBS)
sssp_DEPENDENCIES = $(HPX_DEPS)

if HAVE_MPI
sssp_CFLAGS           += @MPI_CFLAGS@
endif
