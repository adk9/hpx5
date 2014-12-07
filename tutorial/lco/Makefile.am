lcodir                        = $(datarootdir)/doc/@PACKAGE@
lco_PROGRAMS                  = chan probe chanarray chanSendInorder sema \
                                lcoAnd genCount 

chan_SOURCES                  = chan.c 
probe_SOURCES                 = probe.c
chanarray_SOURCES             = chanarray.c
chanSendInorder_SOURCES       = chanSendInorder.c
sema_SOURCES                  = sema.c
lcoAnd_SOURCES                = lcoAnd.c
genCount_SOURCES              = genCount.c

all_LIBS                      = $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS                   = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                     =

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

chan_LDADD                    = $(all_LIBS)
probe_LDADD                   = $(all_LIBS)
chanarray_LDADD               = $(all_LIBS)
chanSendInorder_LDADD         = $(all_LIBS)
sema_LDADD                    = $(all_LIBS)
lcoAnd_LDADD                  = $(all_LIBS)
genCount_LDADD                = $(all_LIBS)
