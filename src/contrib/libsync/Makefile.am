AM_CFLAGS  = @CFLAGS@ -I@PHOTON_SRCDIR@/src/contrib \
		      -I@PHOTON_SRCDIR@/src/contrib/libsync/include

AM_CFLAGS += -fvisibility=hidden

SUBDIRS = arch

noinst_LTLIBRARIES = libsync.la

libsync_la_SOURCES = locks.c sr_barrier.c two_lock_queue.c \
                     chase_lev_ws_deque.c backoff.c lockable_ptr.c \
                     spscq.c clh_lock.c mcs_lock.c

if HAVE_CMPXCHG16B
  libsync_la_SOURCES += ms_queue.c
endif
libsync_la_LIBADD  = arch/libarch.la
