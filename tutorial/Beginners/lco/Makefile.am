lcodir                        = $(datarootdir)/doc/@PACKAGE@
lco_PROGRAMS                  = sema lcoAnd  lcoSetGet lcoAll lcoError \
                                lcoFutureArray 

AM_CPPFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS       		= $(HPX_APPS_LDFLAGS) -no-install
LDADD            		= $(HPX_APPS_LDADD)

sema_SOURCES                  = sema.c
lcoAnd_SOURCES                = lcoAnd.c
lcoSetGet_SOURCES             = lcoSetGet.c
lcoAll_SOURCES                = lcoAll.c
lcoError_SOURCES              = lcoError.c
lcoFutureArray_SOURCES        = lcoFutureArray.c

sema_DEPENDENCIES                    = $(HPX_APPS_DEPS)
lcoAnd_DEPENDENCIES                  = $(HPX_APPS_DEPS)
lcoSetGet_DEPENDENCIES               = $(HPX_APPS_DEPS)
lcoAll_DEPENDENCIES                  = $(HPX_APPS_DEPS)
lcoError_DEPENDENCIES                = $(HPX_APPS_DEPS)
lcoFutureArray_DEPENDENCIES          = $(HPX_APPS_DEPS)
