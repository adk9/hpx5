####################
# Makefile for HPX #
####################

lib_LTLIBRARIES          = libhpx.la

pkgconfigdir             = $(libdir)/pkgconfig
nodist_pkgconfig_DATA    = libhpx.pc

#network
libhpx_la_SOURCES        = parcel/parcelqueue.c parcel/request_buffer.c \
                           parcel/request_list.c parcel/serialization.c \
                           parcel/action.c parcel/hashstr.c parcel/handler.c \
                           parcel/parcel.c \
                           runtime/init.c runtime/locality.c \
                           thread/ctx.c thread/thread.c thread/kthread.c \
                           utils/error.c utils/timer.c utils/config.c \
                           utils/map.c

if CPU_X86_64
    libhpx_la_SOURCES   += thread/arch/x86_64/mctx.S \
                           thread/arch/x86_64/mconfig.S \
                           thread/arch/x86_64/mctx_mk.c

    libhpx_la_arch_x86_64_HEADERS \
                         = $(top_srcdir)/include/hpx/thread/arch/x86_64/*.h
    libhpx_la_arch_x86_64dir \
                         = $(includedir)/hpx/arch/x86_64
endif

if HAVE_NETWORK
    libhpx_la_SOURCES   += network/network.c
                           bootstrap/bootstrap.c
endif

if HAVE_MPI
    libhpx_la_SOURCES   += network/mpi.c
    libhpx_la_SOURCES   += bootstrap/mpirun.c
endif

if HAVE_PMI
    libhpx_la_SOURCES   += bootstrap/pmi.c
endif

if HAVE_PHOTON
    libhpx_la_SOURCES   += network/photon.c
endif

libhpx_ladir             = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_builddir)/include/config.h
libhpx_la_HEADERS        = $(top_srcdir)/include/hpx/*.h

libhpx_la_CPPFLAGS       = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx

libhpx_la_CFLAGS         = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
libhpx_la_CFLAGS        += @CRYPTO_CFLAGS@ @MPI_CFLAGS@ @PHOTON_CFLAGS@
libhpx_la_CFLAGS        += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
libhpx_la_CFLAGS        += @GLIB_CFLAGS@ @PMI_CFLAGS@
libhpx_la_CFLAGS        += -std=c99

libhpx_la_LIBADD         = $(top_builddir)/libsync/libsync.la
libhpx_la_LIBADD        += @MPI_LIBS@ @PHOTON_LIBS@ @CRYPTO_LIBS@
libhpx_la_LIBADD        += @HWLOC_LIBS@ @PAPI_LIBS@ @TCMALLOC_LIBS@
libhpx_la_LIBADD        += @GLIB_LIBS@ @PMI_LIBS@ @PLATFORM_LIBS@
