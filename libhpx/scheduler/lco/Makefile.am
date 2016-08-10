# The LCO library
noinst_LTLIBRARIES  = liblco.la
noinst_HEADERS      = lco.h

liblco_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../ $(LIBHPX_CPPFLAGS)
liblco_la_CFLAGS    = $(LIBHPX_CFLAGS)
liblco_la_CXXFLAGS  = $(LIBHPX_CXXFLAGS)
liblco_la_SOURCES   = allreduce.cpp alltoall.cpp and.cpp future.cpp \
                      gather.cpp gencount.cpp lco.cpp reduce.cpp sema.cpp \
                      user_lco.cpp dataflow.cpp monoid.cpp

