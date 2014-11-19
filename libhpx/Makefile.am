####################
# Makefile for HPX #
####################
SUBDIRS = system util boot gas transport network scheduler

CC=tau_cc.sh

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c call.c par.c action.c debug.c 

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include
libhpx_la_LIBADD   = system/libsystem.la \
                     boot/libboot.la \
                     gas/libgas.la \
                     transport/libtransport.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     util/libutil.la \
                     $(HPX_JEMALLOC_LDADD) \
                     $(HPX_PHOTON_LDADD) \
                     $(HWLOC_EMBEDDED_LDADD)

if HAVE_PAPI
libhpx_la_CFLAGS   = @PAPI_CFLAGS@
libhpx_la_LIBADD  += @PAPI_LIBS@
endif
