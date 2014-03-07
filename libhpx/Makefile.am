####################
# Makefile for HPX #
####################
SUBDIRS = locality network scheduler

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c debug.c

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx
libhpx_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libhpx_la_LIBADD   = locality/liblocality.la network/libnetwork.la scheduler/libscheduler.la
libhpx_la_LDFLAGS  = -pthread

if HAVE_PAPI
libhpx_la_CFLAGS  += @PAPI_CFLAGS@
libhpx_la_LIBADD  += @PAPI_LIBS@
endif

if HAVE_TCMALLOC
libhpx_la_CFLAGS  += @TCMALLOC_CFLAGS@
libhpx_la_LIBADD  += @TCMALLOC_LIBS@
endif
