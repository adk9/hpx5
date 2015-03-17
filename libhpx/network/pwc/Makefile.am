# The isend-irecv network implementations
noinst_LTLIBRARIES	= libpwc.la

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include
libpwc_la_SOURCES  = xport.c segment.c circular_buffer.c eager_buffer.c \
                     send_buffer.c peer.c pwc.c reload.c

if HAVE_PHOTON
libpwc_la_SOURCES += xport_photon.c
endif
