SUBDIRS = arch

# The networking library
noinst_LTLIBRARIES       = libscheduler.la

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libscheduler_la_CFLAGS   = $(LIBHPX_CFLAGS)
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c lco.c future.c \
                           and.c stats.c sema.c gencount.c allreduce.c \
                           reduce.c allgather.c alltoall.c process.c termination.c \
                           user_lco.c 
libscheduler_la_LIBADD   = arch/libarch.la
