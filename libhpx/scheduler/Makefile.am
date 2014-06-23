SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES	     = libscheduler.la

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include
libscheduler_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c chan.c
libscheduler_la_LIBADD   = arch/libarch.la $(top_builddir)/libsync/libsync.la

if GNU_PE_ENV
libscheduler_la_LDFLAGS  = -pthread
else
libscheduler_la_LDFLAGS  = -lpthread
endif

