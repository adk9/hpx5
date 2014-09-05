noinst_appsdir         = $(abs_top_builddir)/apps/

noinst_apps_PROGRAMS   = mpi

mpi_SOURCES  = allgather.c      \
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

mpi_LDFLAGS  = -static
mpi_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

mpi_CPPFLAGS = -I$(top_srcdir)/include
mpi_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
mpi_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

if HAVE_PHOTON
mpi_LDADD   += -lphoton -ljemalloc
endif
