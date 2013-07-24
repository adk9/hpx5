####################
# Makefile for HPX #
####################

AM_CPPFLAGS =

SUBDIRS = .

lib_LTLIBRARIES = libhpx.la

libhpx_la_SOURCES = network/network.c \
                    parcel/handler.c  \
                    runtime/ctx.c     \
		    runtime/init.c    \
		    thread/thread.c   \
		    thread/kthread.c  \
		    util/error.c      \
		    util/timer.c      \
		    util/config.c     \
		    util/map.c

if CPU_X86_64
  ARCH_SRC = thread/arch/x86_64/mctx.S       \
	     thread/arch/x86_64/mconfig.S    \
             thread/arch/x86_64/mctx_mk.c
  libhpx_la_arch_x86_64dir = $(includedir)/hpx/arch/x86_64
  libhpx_la_arch_x86_64_HEADERS = $(top_srcdir)/include/arch/x86_64/*.h
endif

libhpx_la_SOURCES += $(ARCH_SRC)

libhpx_ladir = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_srcdir)/include/config.h
libhpx_la_HEADERS = $(top_srcdir)/include/*.h

if PLATFORM_LINUX
  libhpx_la_LIBADD = -lrt
endif

if HAVE_MPI
MPI_DEFS = -DHAVE_MPI=1
libhpx_la_SOURCES += network/mpi.c
endif

if HAVE_PHOTON
PHOTON_DEFS = -DHAVE_PHOTON=1
libhpx_la_SOURCES += network/photon.c
endif

AM_CPPFLAGS += $(MPI_DEFS) $(PHOTON_DEFS)