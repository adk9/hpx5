threadsdir                  = $(datarootdir)/doc/@PACKAGE@
threads_PROGRAMS            = threadSpawn1 threadSpawn2 threadSpawn3 threadSpawn4 \
                              threadArgPassing threadContinue threadContCleanup \
                              threadSemaphores threadYield threadExit \
                              threadCurrentTarget  

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

all_LIBS                    = $(HPX_JEMALLOC_LIBS) $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_LDFLAGS) -no-install

if HAVE_MPI
AM_CFLAGS                  += @MPI_CFLAGS@
endif

threadSpawn1_LDADD          = $(all_LIBS)
threadSpawn2_LDADD          = $(all_LIBS)
threadSpawn3_LDADD          = $(all_LIBS)
threadSpawn4_LDADD          = $(all_LIBS)
threadArgPassing_LDADD      = $(all_LIBS)
threadContinue_LDADD        = $(all_LIBS)
threadContCleanup_LDADD     = $(all_LIBS)
threadSemaphores_LDADD      = $(all_LIBS)
threadYield_LDADD           = $(all_LIBS)
threadExit_LDADD            = $(all_LIBS)
threadCurrentTarget_LDADD   = $(all_LIBS)

threadSpawn1_DEPENDENCIES          = $(HPX_DEPS)
threadSpawn2_DEPENDENCIES          = $(HPX_DEPS)
threadSpawn3_DEPENDENCIES          = $(HPX_DEPS)
threadSpawn4_DEPENDENCIES          = $(HPX_DEPS)
threadArgPassing_DEPENDENCIES      = $(HPX_DEPS)
threadContinue_DEPENDENCIES        = $(HPX_DEPS)
threadContCleanup_DEPENDENCIES     = $(HPX_DEPS)
threadSemaphores_DEPENDENCIES      = $(HPX_DEPS)
threadYield_DEPENDENCIES           = $(HPX_DEPS)
threadExit_DEPENDENCIES            = $(HPX_DEPS)
threadCurrentTarget_DEPENDENCIES   = $(HPX_DEPS)
