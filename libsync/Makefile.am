####################
# Makefile for HPX #
####################
SUBDIRS = arch

noinst_LTLIBRARIES  = libsync.la

libsync_la_SOURCES  = locks.c sr_barrier.c ms_queue.c chase_lev_ws_deque.c cuckoo.c
libsync_la_CPPFLAGS = -I$(top_srcdir)/include
libsync_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99 
libsync_la_LIBADD   = arch/libarch.la