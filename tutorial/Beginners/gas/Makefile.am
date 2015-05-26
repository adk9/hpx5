gasdir                        = $(datarootdir)/doc/@PACKAGE@
gas_PROGRAMS                  = gasalloc memget memget1 memput \
                                memcpy memput1 memTryPin 

gasalloc_SOURCES              = gasalloc.c
memget_SOURCES                = memget.c
memget1_SOURCES               = memget1.c
memput_SOURCES                = memput.c
memput1_SOURCES               = memput1.c
memcpy_SOURCES                = memcpy.c
memTryPin_SOURCES             = memTryPin.c

all_LIBS                    = $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_RPATH) -no-install

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

gasalloc_LDADD                = $(all_LIBS)
memget_LDADD                  = $(all_LIBS)
memget1_LDADD                 = $(all_LIBS)
memput_LDADD                  = $(all_LIBS)
memput1_LDADD                 = $(all_LIBS)
memcpy_LDADD                  = $(all_LIBS)
memTryPin_LDADD               = $(all_LIBS)

gasalloc_DEPENDENCIES         = $(HPX_DEPS)
memget_DEPENDENCIES           = $(HPX_DEPS)
memget1_DEPENDENCIES          = $(HPX_DEPS)
memput_DEPENDENCIES           = $(HPX_DEPS)
memput1_DEPENDENCIES          = $(HPX_DEPS)
memcpy_DEPENDENCIES           = $(HPX_DEPS)
memTryPin_DEPENDENCIES        = $(HPX_DEPS)
