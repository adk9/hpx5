####################
# Makefile for HPX #
####################
SUBDIRS = system boot transport network scheduler

# The main libhpx library
lib_LTLIBRARIES	   = libhpx.la

libhpx_la_SOURCES  = hpx.c debug.c action.c

libhpx_la_CPPFLAGS = -I$(top_srcdir)/include
libhpx_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libhpx_la_LIBADD   = system/libsystem.la \
                     boot/libboot.la \
                     transport/libtransport.la \
                     network/libnetwork.la \
                     scheduler/libscheduler.la
libhpx_la_LDFLAGS  = -pthread

if HAVE_PAPI
libhpx_la_CFLAGS  += @PAPI_CFLAGS@
libhpx_la_LIBADD  += @PAPI_LIBS@
endif

if HAVE_TCMALLOC
libhpx_la_CFLAGS  += @TCMALLOC_CFLAGS@
libhpx_la_LIBADD  += @TCMALLOC_LIBS@
endif
