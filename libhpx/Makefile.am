####################
# Makefile for HPX #
####################
SUBDIRS = system boot gas transport network scheduler

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c debug.c action.c par.c

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include
libhpx_la_LIBADD   = system/libsystem.la \
                     boot/libboot.la \
                     gas/libgas.la \
                     transport/libtransport.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la \
                     $(HWLOC_EMBEDDED_LDADD)

# if GNU_PE_ENV
# libhpx_la_LDFLAGS  = -pthread
# else
# libhpx_la_LDFLAGS  = -lpthread
# endif

if HAVE_PAPI
libhpx_la_CFLAGS   = @PAPI_CFLAGS@
libhpx_la_LIBADD  += @PAPI_LIBS@
endif
