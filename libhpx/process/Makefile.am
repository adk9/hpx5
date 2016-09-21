# The process infrastructure
noinst_LTLIBRARIES     = libprocess.la
noinst_HEADERS         = allreduce.h Bitmap.h

libprocess_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libprocess_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libprocess_la_SOURCES  = broadcast.cpp process.cpp Bitmap.cpp \
                         continuation.cpp reduce.cpp allreduce.cpp \
                         allreduce_actions.cpp allreduce_glue.cpp
