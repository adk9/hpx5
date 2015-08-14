####################
# Makefile for HPX #
####################

lib_LTLIBRARIES  = libapex_hpx.la
libapex_hpx_la_LIBADD  = @APEX_LIBS@

libapex_hpx_la_CFLAGS = $(LIBFFI_CFLAGS) $(LIBHPX_CFLAGS) @APEX_CFLAGS@
libapex_hpx_la_SOURCES  = apex_global_hpx.c

