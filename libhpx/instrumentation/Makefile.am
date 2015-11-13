noinst_LTLIBRARIES = libinstrumentation.la
noinst_HEADERS     = file_header.h logtable.h

libinstrumentation_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libinstrumentation_la_CFLAGS   = $(LIBHPX_CFLAGS)

if HAVE_PAPI
libinstrumentation_la_SOURCES  = file_header.c instrumentation.c \
                                 instrumentation_events.c logtable.c \
                                 prof_papi.c
else
if ENABLE_INSTRUMENTATION
libinstrumentation_la_SOURCES  = file_header.c instrumentation.c \
                                 instrumentation_events.c logtable.c \
                                 prof_basic.c
else
libinstrumentation_la_SOURCES  = file_header.c instrumentation.c \
                                 instrumentation_events.c logtable.c \
                                 prof_empty.c

endif

endif

