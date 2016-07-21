if HAVE_NETWORK
SUBDIRS = isir pwc
endif

# The networking library
noinst_LTLIBRARIES     = libnetwork.la
noinst_HEADERS         = inst.h smp.h coalesced.h compressed.h

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libnetwork_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libnetwork_la_SOURCES  = locality.cpp \
                         network.cpp \
                         parcel.cpp \
                         parcel_block.cpp \
                         hpx_parcel_glue.cpp \
                         smp.cpp inst.cpp \
                         coalesced.cpp \
                         string.cpp \
                         compressed.cpp

if HAVE_NETWORK
libnetwork_la_LIBADD   = isir/libisir.la \
                         pwc/libpwc.la
endif
