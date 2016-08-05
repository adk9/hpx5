# The isend-irecv network implementations
noinst_LTLIBRARIES = libpwc.la
noinst_HEADERS     = CircularBuffer.h commands.h parcel_emulation.h \
                     registered.h send_buffer.h xport.h 

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libpwc_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libpwc_la_SOURCES  = reload.cpp \
                     xport.cpp \
                     send_buffer.cpp \
                     CircularBuffer.cpp \
                     lco_get.cpp \
                     lco_wait.cpp \
                     commands.cpp \
                     rendezvous_send.cpp \
                     DMAStringOps.cpp \
                     PWCNetwork.cpp

if HAVE_PHOTON
libpwc_la_SOURCES += xport_photon.cpp
endif

if HAVE_JEMALLOC
libpwc_la_SOURCES += jemalloc_registered.cpp
endif

if HAVE_TBBMALLOC
libpwc_la_SOURCES += tbbmalloc_registered.cpp
endif

if HAVE_DLMALLOC
libpwc_la_SOURCES += dlmalloc_registered.cpp
endif
