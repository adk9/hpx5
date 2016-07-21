SUBDIRS = arch lco

# The scheduler library
noinst_LTLIBRARIES       = libscheduler.la
noinst_HEADERS           = cvar.h thread.h

libscheduler_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libscheduler_la_CFLAGS   = $(LIBHPX_CFLAGS)
libscheduler_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libscheduler_la_SOURCES  = cvar.c scheduler.c worker.c thread.c \
                           sigmask.c execute_thread.cpp
libscheduler_la_LIBADD   = arch/libarch.la lco/liblco.la

if ENABLE_INSTRUMENTATION
libscheduler_la_SOURCES += events.c
endif
