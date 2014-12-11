threadsdir                  = $(datarootdir)/doc/@PACKAGE@
threads_PROGRAMS            = threadSpawn1 threadSpawn2 threadSpawn3 threadSpawn4 \
                              threadArgPassing threadContinue threadContCleanup \
                              threadSemaphores threadYield threadExit \
                              threadCurrentTarget threadCurrentCont 

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
threadCurrentCont_SOURCES   = threadCurrentCont.c

all_LIBS                    = $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =

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
threadCurrentCont_LDADD     = $(all_LIBS)
