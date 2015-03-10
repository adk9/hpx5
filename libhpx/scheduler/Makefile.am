SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES       = libscheduler.la

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c chan.c allreduce.c \
                           reduce.c allgather.c alltoall.c process.c termination.c \
                           user_lco.c netfuture.c
libscheduler_la_LIBADD   = arch/libarch.la #$(top_builddir)/libsync/libsync.la

if HAVE_APEX
libscheduler_la_CPPFLAGS  += @APEX_CFLAGS@ -I$(top_srcdir)/libapex
libscheduler_la_LIBADD  += @APEX_LIBS@ $(top_builddir)/libapex/libapex_hpx.la
endif
