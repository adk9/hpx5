SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES       = libscheduler.la

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include -DJEMALLOC_RENAME=libhpx_global_
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c chan.c allreduce.c \
                           allgather.c alltoall.c process.c termination.c
libscheduler_la_LIBADD   = arch/libarch.la $(top_builddir)/libsync/libsync.la

if HAVE_PHOTON
libscheduler_la_SOURCES  += netfuture.c
endif
