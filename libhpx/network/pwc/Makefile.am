# The isend-irecv network implementations
noinst_LTLIBRARIES	= libpwc.la

libpwc_la_CPPFLAGS = -I$(top_srcdir)/include
libpwc_la_SOURCES  = pwc.c reload.c xport.c send_buffer.c circular_buffer.c 

if HAVE_PHOTON
libpwc_la_SOURCES += xport_photon.c
endif
