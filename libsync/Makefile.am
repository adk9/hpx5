####################
# Makefile for HPX #
####################
SUBDIRS = arch

noinst_LTLIBRARIES  = libsync.la

libsync_la_SOURCES  = locks.c sr_barrier.c ms_queue.c
libsync_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/sync
libsync_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99 
libsync_la_LIBADD   = arch/libarch.la