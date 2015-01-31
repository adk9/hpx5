cpidir                          = $(datarootdir)/doc/@PACKAGE@
cpi_PROGRAMS                    = cpi

cpi_SOURCES                     = cpi.c 

all_LIBS                        = $(HPX_LDADD)

AM_CPPFLAGS                     = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                       =

if HAVE_MPI
AM_CFLAGS                      += @MPI_CFLAGS@
endif

cpi_LDADD                       = $(all_LIBS)

cpi_DEPENDENCIES                = $(all_LIBS)
