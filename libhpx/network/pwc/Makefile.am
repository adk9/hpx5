# The isend-irecv network implementations
noinst_LTLIBRARIES = libpwc.la
noinst_HEADERS     = CircularBuffer.h Commands.h ReloadParcelEmulator.h \
                     registered.h SendBuffer.h PhotonTransport.h ParcelBlock.h

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libpwc_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libpwc_la_SOURCES  = ParcelBlock.cpp \
                     SendBuffer.cpp \
                     CircularBuffer.cpp \
                     lco_get.cpp \
                     lco_wait.cpp \
                     Commands.cpp \
                     rendezvous_send.cpp \
                     DMAStringOps.cpp \
                     PWCNetwork.cpp \
                     ReloadParcelEmulator.cpp \
                     PhotonTransport.cpp

if HAVE_JEMALLOC
libpwc_la_SOURCES += jemalloc_registered.cpp
endif

if HAVE_TBBMALLOC
libpwc_la_SOURCES += tbbmalloc_registered.cpp
endif

if HAVE_DLMALLOC
libpwc_la_SOURCES += dlmalloc_registered.cpp
endif
