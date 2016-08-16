# The LCO library
noinst_LTLIBRARIES  = liblco.la
noinst_HEADERS      = lco.h

liblco_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../ $(LIBHPX_CPPFLAGS)
liblco_la_CFLAGS    = $(LIBHPX_CFLAGS)
liblco_la_CXXFLAGS  = $(LIBHPX_CXXFLAGS)
liblco_la_SOURCES   = lco.cpp and.cpp future.cpp sema.cpp allreduce.cpp \
                      dataflow.cpp gather.cpp reduce.cpp alltoall.cpp \
                      gencount.cpp user_lco.cpp monoid.cpp
