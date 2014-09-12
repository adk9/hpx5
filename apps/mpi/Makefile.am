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

noinst_apps_PROGRAMS   = hpxmpi

hpxmpi_SOURCES  = main.c
hpxmpi_LDFLAGS  = -static

hpxmpi_CPPFLAGS = -I$(top_srcdir)/include
hpxmpi_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
hpxmpi_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@
hpxmpi_LDADD = $(top_builddir)/apps/mpi/libhpxmpi.la $(top_builddir)/libhpx/libhpx.la

if HAVE_PHOTON
libhpxmpi_la_LIBADD  += @PHOTON_LIBS@ @MPI_LIBS@
hpxmpi_LDADD += @PHOTON_LIBS@ @MPI_LIBS@
endif
