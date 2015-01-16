noinst_appsdir         = $(datarootdir)/apps/

noinst_LTLIBRARIES = libhpxmpi.la

libhpxmpi_la_SOURCES = allgather.c\
               allreduce.c      \
               alltoall.c       \
               alltoallv.c      \
               bcast.c          \
               gather.c         \
               gatherv.c        \
               logging.c        \
               mpi_system.c     \
               mpi_wtime.c      \
               p2p.c            \
               reduce.c         \
               scatterv.c       \
               sendrecv.c

libhpxmpi_la_CPPFLAGS = -I$(top_srcdir)/include
libhpxmpi_la_LIBADD   = $(top_builddir)/libhpx/libhpx.la

libhpxmpi_la_CFLAGS   = -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable

if HAVE_PHOTON
libhpxmpi_la_CFLAGS  += $(PHOTON_CFLAGS)
libhpxmpi_la_LIBADD  += @MPI_LIBS@ 
endif

noinst_apps_PROGRAMS  = hpxmpi

hpxmpi_SOURCES  = main.c
hpxmpi_LDFLAGS  = 

hpxmpi_CPPFLAGS = -I$(top_srcdir)/include
hpxmpi_CFLAGS   = @HWLOC_CFLAGS@ @PAPI_CFLAGS@
hpxmpi_LDADD    = libhpxmpi.la $(HPX_LDADD)
hpxmpi_DEPENDENCIES = $(HPX_LDADD)

if HAVE_PHOTON
hpxmpi_CFLAGS   += $(PHOTON_CFLAGS)
hpxmpi_LDADD    += @MPI_LIBS@
endif
