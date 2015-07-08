HeatSeqdir                      = $(datarootdir)/doc/@PACKAGE@
HeatSeq_PROGRAMS                = heat_1

AM_CPPFLAGS			= -D_GNU_SOURCE $(LIBFFI_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(LIBHPX_LDADD)

heat_1_SOURCES                  = heat_1.c 
heat_1_DEPENDENCIES             = $(LIBHPX_DEPS)
