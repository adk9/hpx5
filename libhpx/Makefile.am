####################
# Makefile for HPX #
####################
SUBDIRS = system util boot memory gas network scheduler instrumentation 

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c call.c bcast.c par.c action.c debug.c

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include
libhpx_la_LDFLAGS  = $(HPX_JEMALLOC_LDFLAGS)
libhpx_la_LIBADD   = boot/libboot.la \
                     memory/libmemory.la \
                     gas/libgas.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     util/libutil.la \
                     system/libsystem.la \
                     instrumentation/libinstrumentation.la \
                     $(top_builddir)/libsync/libsync.la \
                     $(HPX_LIBFFI_LIBS) \
                     $(HPX_HWLOC_LIBS) \
                     $(HPX_PHOTON_LIBS) \
                     $(HPX_JEMALLOC_LIBS)
