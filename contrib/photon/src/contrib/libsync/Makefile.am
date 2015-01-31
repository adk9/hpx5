AM_CFLAGS = @CFLAGS@ -I@PHOTON_SRCDIR@/src/contrib \
		     -I@PHOTON_SRCDIR@/src/contrib/libsync/include

SUBDIRS = arch

noinst_LTLIBRARIES = libsync.la

libsync_la_SOURCES = locks.c backoff.c ms_queue.c two_lock_queue.c
libsync_la_LIBADD  = arch/libarch.la
