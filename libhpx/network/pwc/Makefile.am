# The isend-irecv network implementations
if HAVE_PHOTON
SUBDIRS = nbc-photon
endif

noinst_LTLIBRARIES = libpwc.la
noinst_HEADERS     = circular_buffer.h commands.h parcel_emulation.h pwc.h registered.h send_buffer.h xport.h 

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include -Inbc-photon/ $(LIBHPX_CPPFLAGS)
libpwc_la_CFLAGS   = $(LIBHPX_CFLAGS)
libpwc_la_SOURCES  = pwc.c \
                     reload.c \
                     xport.c \
                     send_buffer.c \
                     circular_buffer.c \
                     lco_get.c \
                     lco_wait.c \
                     commands.c \
                     rendezvous_send.c

if HAVE_PHOTON
libpwc_la_SOURCES += xport_photon.c
endif

if HAVE_JEMALLOC
libpwc_la_SOURCES += jemalloc_registered.c
endif

if HAVE_TBBMALLOC
noinst_LTLIBRARIES		   += libtbbmalloc.la
libtbbmalloc_la_CPPFLAGS	= -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libtbbmalloc_la_CXXFLAGS 	= $(LIBHPX_CXXFLAGS)
libtbbmalloc_la_SOURCES  	= tbbmalloc_registered.cc
libpwc_la_LIBADD			= nbc-photon/libnbc.la libtbbmalloc.la
else
#libpwc_la_LIBADD			= nbc-photon/libnbc.la 
endif

if HAVE_DLMALLOC
libpwc_la_SOURCES += dlmalloc_registered.c
endif
