####################
# Makefile for HPX #
####################
SUBDIRS = arch

noinst_LTLIBRARIES  = libsync.la

libsync_la_SOURCES  = locks.c sr_barrier.c ms_queue.c two_lock_queue.c \
                      chase_lev_ws_deque.c cuckoo.c backoff.c lockable_ptr.c
libsync_la_CPPFLAGS = -I$(top_srcdir)/include
libsync_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
libsync_la_LIBADD   = arch/libarch.la
