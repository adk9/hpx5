AM_CFLAGS = @CFLAGS@ -I@PHOTON_SRCDIR@/src/contrib

SUBDIRS = arch

noinst_LTLIBRARIES = libsync.la

libsync_la_SOURCES = locks.c backoff.c
libsync_la_LIBADD  = arch/libarch.la
