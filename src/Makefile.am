####################
# Makefile for HPX #
####################

lib_LTLIBRARIES          = libhpx.la

libhpx_la_SOURCES        = network/network.c \
                           bootstrap/bootstrap.c \
                           parcel/action.c \
                           parcel/handler.c \
                           parcel/parcel.c \
                           runtime/init.c    \
                           runtime/locality.c \
                           thread/ctx.c     \
                           thread/thread.c   \
                           thread/kthread.c  \
                           util/error.c      \
                           util/timer.c      \
                           util/config.c     \
                           util/map.c        \
                           sync/locks.c

if CPU_X86_64
libhpx_la_arch_x86_64_HEADERS = $(top_srcdir)/include/hpx/arch/x86_64/*.h
libhpx_la_arch_x86_64dir = $(includedir)/hpx/arch/x86_64
ARCH_SRC                 = thread/arch/x86_64/mctx.S       \
                           thread/arch/x86_64/mconfig.S    \
                           thread/arch/x86_64/mctx_mk.c \
                           arch/x86_64/sync/nop.c
endif

libhpx_la_SOURCES       += $(ARCH_SRC)

libhpx_ladir             = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_builddir)/include/config.h
libhpx_la_HEADERS        = $(top_srcdir)/include/hpx/*.h
libhpx_la_CPPFLAGS       = -I$(top_srcdir)/include -std=c99
libhpx_la_CPPFLAGS      += $(MPI_DEFS) $(PHOTON_DEFS) $(NETWORK_DEFS)
libhpx_la_CFLAGS         = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
libhpx_la_CFLAGS        += $(HPX_F_LTO)
libhpx_la_CFLAGS        += @PAPI_CFLAGS@ @HWLOC_CFLAGS@ @TCMALLOC_CFLAGS@
libhpx_la_LIBADD         = @PAPI_LIBS@ @HWLOC_LIBS@ @TCMALLOC_LIBS@

if PLATFORM_LINUX
libhpx_la_LIBADD += -lrt
endif

if HAVE_TCMALLOC
libhpx_la_CFLAGS += @TCMALLOC_CFLAGS@
libhpx_la_LIBADD += @TCMALLOC_LIBS@
endif

if HAVE_MPI
MPI_DEFS = -DHAVE_MPI=1
libhpx_la_SOURCES += network/mpi.c
libhpx_la_SOURCES += bootstrap/mpirun.c
endif

if HAVE_PHOTON
  PHOTON_DEFS = -DHAVE_PHOTON=1
  libhpx_la_SOURCES += network/photon.c
  libhpx_la_LIBADD += -lphoton -libverbs -lrdmacm
endif

if HAVE_NETWORK
  NETWORK_DEFS = -DHAVE_NETWORK=1
endif


