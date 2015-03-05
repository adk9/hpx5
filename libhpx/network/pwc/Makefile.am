# The isend-irecv network implementations
noinst_LTLIBRARIES	= libpwc.la

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include
libpwc_la_SOURCES  = segment.c circular_buffer.c pwc_buffer.c eager_buffer.c \
                     send_buffer.c peer.c pwc.c commands.c transport.c

if HAVE_PHOTON
libpwc_la_SOURCES  += transport_photon.c
endif
