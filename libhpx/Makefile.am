####################
# Makefile for HPX #
####################
SUBDIRS = system util boot memory gas network scheduler process instrumentation 

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libhpx_la_CFLAGS   = $(LIBHPX_CFLAGS)
libhpx_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libhpx_la_SOURCES  = libhpx.c hpx.c call.c map.c par.c action.c debug.c version.c

libhpx_la_LDFLAGS  = $(LIBHPX_LDFLAGS)
libhpx_la_LIBADD   = $(LIBHPX_LIBS) $(LIBHPX_LIBADD) \
                     $(top_builddir)/libsync/libsync.la \
                     boot/libboot.la \
                     memory/libmemory.la \
                     gas/libgas.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     util/libutil.la \
                     system/libsystem.la \
                     process/libprocess.la \
                     instrumentation/libinstrumentation.la
