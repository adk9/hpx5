cpidir                          = $(datarootdir)/doc/@PACKAGE@
cpi_PROGRAMS                    = cpi

AM_CPPFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS       		= -no-install
LDADD            		= $(HPX_APPS_LDADD)

cpi_SOURCES                     = cpi.c 
cpi_DEPENDENCIES                = $(HPX_APPS_DEPS)
