cpidir                          = $(datarootdir)/doc/@PACKAGE@
cpi_PROGRAMS                    = cpi

AM_CPPFLAGS			= -D_GNU_SOURCE $(LIBFFI_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(LIBHPX_LDADD)

cpi_SOURCES                     = cpi.c 
cpi_DEPENDENCIES                = $(LIBHPX_DEPS)
