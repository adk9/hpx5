noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = hpxmpi

hpxmpi_SOURCES  = allgather.c      \
               allreduce.c      \
               alltoall.c       \
               alltoallv.c      \
               bcast.c          \
               gather.c         \
               gatherv.c        \
               logging.c        \
               main.c           \
               mpi_system.c     \
               mpi_wtime.c      \
               p2p.c            \
               reduce.c         \
               scatterv.c       \
               sendrecv.c

hpxmpi_LDFLAGS  = -static
hpxmpi_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

hpxmpi_CPPFLAGS = -I$(top_srcdir)/include
hpxmpi_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
hpxmpi_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

if HAVE_PHOTON
hpxmpi_LDADD   += -lphoton -ljemalloc
endif
