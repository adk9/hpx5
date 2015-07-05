lcodir                        = $(datarootdir)/doc/@PACKAGE@
lco_PROGRAMS                  = sema lcoAnd  lcoSetGet lcoAll lcoError \
                                lcoFutureArray 

AM_CPPFLAGS					= -D_GNU_SOURCE -I$(top_srcdir)/include
AM_LDFLAGS                  = $(LIBHPX_LDFLAGS) -no-install
LDADD						= $(LIBHPX_LDADD)

sema_SOURCES                  = sema.c
lcoAnd_SOURCES                = lcoAnd.c
lcoSetGet_SOURCES             = lcoSetGet.c
lcoAll_SOURCES                = lcoAll.c
lcoError_SOURCES              = lcoError.c
lcoFutureArray_SOURCES        = lcoFutureArray.c

sema_DEPENDENCIES                    = $(LIBHPX_DEPS)
lcoAnd_DEPENDENCIES                  = $(LIBHPX_DEPS)
lcoSetGet_DEPENDENCIES               = $(LIBHPX_DEPS)
lcoAll_DEPENDENCIES                  = $(LIBHPX_DEPS)
lcoError_DEPENDENCIES                = $(LIBHPX_DEPS)
lcoFutureArray_DEPENDENCIES          = $(LIBHPX_DEPS)
