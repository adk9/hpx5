lcodir                        = $(datarootdir)/doc/@PACKAGE@
lco_PROGRAMS                  = sema lcoAnd  lcoSetGet lcoAll lcoError \
                                lcoFutureArray 

sema_SOURCES                  = sema.c
lcoAnd_SOURCES                = lcoAnd.c
lcoSetGet_SOURCES             = lcoSetGet.c
lcoAll_SOURCES                = lcoAll.c
lcoError_SOURCES              = lcoError.c
lcoFutureArray_SOURCES        = lcoFutureArray.c

all_LIBS                    = $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_RPATH) -no-install

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

sema_LDADD                    = $(all_LIBS)
lcoAnd_LDADD                  = $(all_LIBS)
lcoSetGet_LDADD               = $(all_LIBS)
lcoAll_LDADD                  = $(all_LIBS)
lcoError_LDADD                = $(all_LIBS)
lcoFutureArray_LDADD          = $(all_LIBS)

sema_DEPENDENCIES                    = $(HPX_DEPS)
lcoAnd_DEPENDENCIES                  = $(HPX_DEPS)
lcoSetGet_DEPENDENCIES               = $(HPX_DEPS)
lcoAll_DEPENDENCIES                  = $(HPX_DEPS)
lcoError_DEPENDENCIES                = $(HPX_DEPS)
lcoFutureArray_DEPENDENCIES          = $(HPX_DEPS)
