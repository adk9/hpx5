HeatSeqdir                      = $(datarootdir)/doc/@PACKAGE@
HeatSeq_PROGRAMS                = heat_1

heat_1_SOURCES                  = heat_1.c 

all_LIBS                    = $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_RPATH) -no-install

if HAVE_MPI
AM_CFLAGS                      += @MPI_CFLAGS@
endif

heat_1_LDADD                    = $(all_LIBS)

heat_1_DEPENDENCIES             = $(HPX_DEPS)
