threadsdir                  = $(datarootdir)/doc/@PACKAGE@
threads_PROGRAMS            = threadSpawn1 threadSpawn2 threadSpawn3 threadSpawn4 \
                              threadArgPassing threadContinue threadContCleanup \
                              threadSemaphores threadYield threadExit \
                              threadCurrentTarget  

AM_CPPFLAGS			= -D_GNU_SOURCE $(HPX_APPS_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(HPX_APPS_LDADD)

threadSpawn1_SOURCES        = threadSpawn1.c
threadSpawn2_SOURCES        = threadSpawn2.c
threadSpawn3_SOURCES        = threadSpawn3.c 
threadSpawn4_SOURCES        = threadSpawn4.c
threadArgPassing_SOURCES    = threadArgPassing.c
threadContinue_SOURCES      = threadContinue.c
threadContCleanup_SOURCES   = threadContCleanup.c
threadSemaphores_SOURCES    = threadSemaphores.c
threadYield_SOURCES         = threadYield.c
threadExit_SOURCES          = threadExit.c
threadCurrentTarget_SOURCES = threadCurrentTarget.c

threadSpawn1_DEPENDENCIES          = $(HPX_APPS_DEPS)
threadSpawn2_DEPENDENCIES          = $(HPX_APPS_DEPS)
threadSpawn3_DEPENDENCIES          = $(HPX_APPS_DEPS)
threadSpawn4_DEPENDENCIES          = $(HPX_APPS_DEPS)
threadArgPassing_DEPENDENCIES      = $(HPX_APPS_DEPS)
threadContinue_DEPENDENCIES        = $(HPX_APPS_DEPS)
threadContCleanup_DEPENDENCIES     = $(HPX_APPS_DEPS)
threadSemaphores_DEPENDENCIES      = $(HPX_APPS_DEPS)
threadYield_DEPENDENCIES           = $(HPX_APPS_DEPS)
threadExit_DEPENDENCIES            = $(HPX_APPS_DEPS)
threadCurrentTarget_DEPENDENCIES   = $(HPX_APPS_DEPS)
