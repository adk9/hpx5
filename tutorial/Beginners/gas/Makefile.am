gasdir                        = $(datarootdir)/doc/@PACKAGE@
gas_PROGRAMS                  = gasalloc memget memget1 memput \
                                memcpy memput1 memTryPin 

AM_CPPFLAGS			= -D_GNU_SOURCE $(LIBFFI_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(LIBHPX_LDADD)

gasalloc_SOURCES              = gasalloc.c
memget_SOURCES                = memget.c
memget1_SOURCES               = memget1.c
memput_SOURCES                = memput.c
memput1_SOURCES               = memput1.c
memcpy_SOURCES                = memcpy.c
memTryPin_SOURCES             = memTryPin.c

gasalloc_DEPENDENCIES         = $(LIBHPX_DEPS)
memget_DEPENDENCIES           = $(LIBHPX_DEPS)
memget1_DEPENDENCIES          = $(LIBHPX_DEPS)
memput_DEPENDENCIES           = $(LIBHPX_DEPS)
memput1_DEPENDENCIES          = $(LIBHPX_DEPS)
memcpy_DEPENDENCIES           = $(LIBHPX_DEPS)
memTryPin_DEPENDENCIES        = $(LIBHPX_DEPS)
