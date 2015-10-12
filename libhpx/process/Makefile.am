# The process infrastructure
noinst_LTLIBRARIES       = libprocess.la

libprocess_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libprocess_la_CFLAGS   = $(LIBHPX_CFLAGS)
libprocess_la_SOURCES  = allreduce.c broadcast.c process.c termination.c
