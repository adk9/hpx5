gasdir                        = $(datarootdir)/doc/@PACKAGE@
gas_PROGRAMS                  = gasalloc memget memget1 memput \
                                memcpy memput1 memTryPin 

AM_CPPFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS       		= $(HPX_APPS_LDFLAGS) -no-install
LDADD            		= $(HPX_APPS_LDADD)

gasalloc_SOURCES              = gasalloc.c
memget_SOURCES                = memget.c
memget1_SOURCES               = memget1.c
memput_SOURCES                = memput.c
memput1_SOURCES               = memput1.c
memcpy_SOURCES                = memcpy.c
memTryPin_SOURCES             = memTryPin.c

gasalloc_DEPENDENCIES         = $(HPX_APPS_DEPS)
memget_DEPENDENCIES           = $(HPX_APPS_DEPS)
memget1_DEPENDENCIES          = $(HPX_APPS_DEPS)
memput_DEPENDENCIES           = $(HPX_APPS_DEPS)
memput1_DEPENDENCIES          = $(HPX_APPS_DEPS)
memcpy_DEPENDENCIES           = $(HPX_APPS_DEPS)
memTryPin_DEPENDENCIES        = $(HPX_APPS_DEPS)
