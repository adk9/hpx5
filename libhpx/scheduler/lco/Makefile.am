# The LCO library
noinst_LTLIBRARIES  = liblco.la
noinst_HEADERS      = lco.h

liblco_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(srcdir)/../ $(LIBHPX_CPPFLAGS)
liblco_la_CFLAGS    = $(LIBHPX_CFLAGS)
liblco_la_CXXFLAGS  = $(LIBHPX_CXXFLAGS)
liblco_la_SOURCES   = allreduce.c alltoall.c and.c future.c \
                      gather.c gencount.c lco.c reduce.c sema.c \
                      user_lco.c dataflow.c monoid.c

