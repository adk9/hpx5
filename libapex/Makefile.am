####################
# Makefile for HPX #
####################

lib_LTLIBRARIES  = libapex_hpx.la
libapex_hpx_la_LIBADD  = @APEX_LIBS@

libapex_hpx_la_CFLAGS = -I. -I$(top_srcdir)/include -I$(top_srcdir)/libapex @APEX_CFLAGS@
libapex_hpx_la_SOURCES  = apex_global_hpx.c

