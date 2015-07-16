AM_CFLAGS = @CFLAGS@ -I@PHOTON_SRCDIR@/src/contrib \
		     -I@PHOTON_SRCDIR@/src/contrib/libsync/include

SUBDIRS = arch

noinst_LTLIBRARIES = libsync.la

libsync_la_SOURCES = locks.c backoff.c two_lock_queue.c
if HAVE_CMPXCHG16B
  libsync_la_SOURCES += ms_queue.c
endif
libsync_la_LIBADD  = arch/libarch.la
