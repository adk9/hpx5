# The isend-irecv network implementations
noinst_LTLIBRARIES  = libisir.la
noinst_HEADERS      = emulate_pwc.h IRecvBuffer.h isend_buffer.h isir.h parcel_utils.h xport.h

libisir_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libisir_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libisir_la_SOURCES  = xport.cpp \
                      funneled.cpp \
                      isend_buffer.cpp \
                      IRecvBuffer.cpp \
                      emulate_pwc.cpp \
                      lco_get.cpp \
                      lco_wait.cpp

if HAVE_MPI
libisir_la_SOURCES += xport_mpi.cpp
endif
