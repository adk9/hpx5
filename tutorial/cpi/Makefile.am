cpidir                          = $(datarootdir)/doc/@PACKAGE@
cpi_PROGRAMS                    = cpi

cpi_SOURCES                     = cpi.c 

all_LIBS                    = $(HPX_JEMALLOC_LIBS) $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_LDFLAGS) -no-install

if HAVE_MPI
AM_CFLAGS                       += @MPI_CFLAGS@
endif

cpi_LDADD                        = $(all_LIBS)

cpi_DEPENDENCIES                = $(HPX_DEPS)
