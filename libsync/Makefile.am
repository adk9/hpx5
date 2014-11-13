####################
# Makefile for HPX #
####################
SUBDIRS = arch

noinst_LTLIBRARIES  = libsync.la

libsync_la_SOURCES  = locks.c sr_barrier.c ms_queue.c two_lock_queue.c \
                      chase_lev_ws_deque.c cuckoo.c backoff.c lockable_ptr.c \
                      spscq.c clh_lock.c
libsync_la_CPPFLAGS = -I$(top_srcdir)/include
libsync_la_LIBADD   = arch/libarch.la
