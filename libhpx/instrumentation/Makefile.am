noinst_LTLIBRARIES = libinstrumentation.la
noinst_HEADERS     = file.h

libinstrumentation_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libinstrumentation_la_CFLAGS   = $(LIBHPX_CFLAGS)

libinstrumentation_la_SOURCES  = file_header.c instrumentation.c \
                                 metadata.c file.c console.c stats.c
