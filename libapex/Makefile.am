####################
# Makefile for HPX #
####################

lib_LTLIBRARIES  = libapex_hpx.la

libapex_hpx_la_CFLAGS  		= -I$(top_srcdir)/include $(LIBFFI_CFLAGS) $(APEX_CFLAGS)
libapex_hpx_la_SOURCES 		= apex_global_hpx.c
libapex_hpx_la_LIBADD  		= $(LIBHPX_LDADD) 
libapex_hpx_la_DEPENDENCIES	= $(LIBHPX_DEPS)
