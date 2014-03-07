SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES	     = libscheduler.la

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx -I$(srcdir)/..
libscheduler_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libscheduler_la_SOURCES  = scheduler.c thread.c lco.c future.c
libscheduler_la_LIBADD   = arch/libarch.la $(top_builddir)/libsync/libsync.la
libscheduler_la_LDFLAGS  = -pthread