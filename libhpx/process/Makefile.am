# The process infrastructure
noinst_LTLIBRARIES       = libprocess.la

libprocess_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libprocess_la_CFLAGS   = $(LIBHPX_CFLAGS)
libprocess_la_SOURCES  = broadcast.c process.c termination.c \
                         flat_allreduce.c flat_reduce.c \
                         flat_continuation.c
