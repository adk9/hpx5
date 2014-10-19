SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES       = libscheduler.la

CC = tau_cc.sh

libscheduler_la_CPPFLAGS = -I/u/jayaajay/tau-2.23.2b3/include -I$(top_srcdir)/include
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c chan.c allreduce.c \
                           allgather.c alltoall.c process.c termination.c
libscheduler_la_LIBADD   = arch/libarch.la $(top_builddir)/libsync/libsync.la

if HAVE_PHOTON
libscheduler_la_SOURCES  += netfuture.c
endif
