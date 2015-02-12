####################
# Makefile for HPX #
####################
SUBDIRS = system util boot gas transport network scheduler

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c call.c par.c action.c debug.c 

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include
libhpx_la_LIBADD   = $(HPX_LIBFFI_LDADD) \
                     $(HWLOC_EMBEDDED_LDADD) \
                     boot/libboot.la \
                     gas/libgas.la \
                     transport/libtransport.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     util/libutil.la \
                     system/libsystem.la \
		     $(HPX_JEMALLOC_AS_MALLOC_LIBS) \
	             $(HPX_JEMALLOC_AS_MALLOC_LIBTOOL)

if HAVE_PAPI
libhpx_la_CFLAGS   = @PAPI_CFLAGS@
libhpx_la_LIBADD  += @PAPI_LIBS@
endif

if ENABLE_INSTRUMENTATION
SUBDIRS += instrumentation
libhpx_la_LIBADD += instrumentation/libinstrumentation.la
endif
