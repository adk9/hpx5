# The process infrastructure
noinst_LTLIBRARIES     = libprocess.la
noinst_HEADERS         = allreduce.h Bitmap.h

libprocess_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libprocess_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libprocess_la_CFLAGS   = $(LIBHPX_CFLAGS)
libprocess_la_SOURCES  = broadcast.c process.cpp Bitmap.cpp \
                         continuation.c reduce.cpp allreduce.cpp \
                         allreduce_actions.c allreduce_glue.c
