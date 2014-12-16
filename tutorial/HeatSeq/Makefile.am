HeatSeqdir                      = $(datarootdir)/doc/@PACKAGE@
HeatSeq_PROGRAMS                = heat_1

heat_1_SOURCES                  = heat_1.c 

all_LIBS                        = $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS                     = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                       =

if HAVE_MPI
AM_CFLAGS                      += @MPI_CFLAGS@
endif

heat_1_LDADD                    = $(all_LIBS)
