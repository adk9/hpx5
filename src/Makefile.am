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
                           utils/map.c \
                           sync/locks.c

if CPU_X86_64

    libhpx_la_SOURCES   += thread/arch/x86_64/mctx.S \
                           thread/arch/x86_64/mconfig.S \
                           thread/arch/x86_64/mctx_mk.c \
                           arch/x86_64/sync/nop.c

    libhpx_la_arch_x86_64_HEADERS \
                         = $(top_srcdir)/include/hpx/thread/arch/x86_64/*.h
    libhpx_la_arch_x86_64dir \
                         = $(includedir)/hpx/arch/x86_64
endif

if HAVE_NETWORK
    NETWORK_DEFS         = -DHAVE_NETWORK=1
    libhpx_la_SOURCES   += network/network.c
    libhpx_la_SOURCES   += bootstrap/bootstrap.c
endif

if HAVE_MPI
    MPI_DEFS             = -DHAVE_MPI=1
    libhpx_la_SOURCES   += network/mpi.c
    libhpx_la_SOURCES   += bootstrap/mpirun.c
endif

if HAVE_PHOTON
    PHOTON_DEFS              = -DHAVE_PHOTON=1
    libhpx_la_SOURCES       += network/photon.c
endif

libhpx_ladir             = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_builddir)/include/config.h
libhpx_la_HEADERS        = $(top_srcdir)/include/hpx/*.h

libhpx_la_CPPFLAGS       = -I$(top_srcdir)/include
libhpx_la_CPPFLAGS      += $(NETWORK_DEFS) $(MPI_DEFS) $(PHOTON_DEFS)

libhpx_la_CFLAGS         = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
libhpx_la_CFLAGS        += $(HPX_OPT_CFLAGS) $(HPX_F_LTO) $(HPX_SYM_CFLAGS)
libhpx_la_CFLAGS        += @LIBCRYPTO_CFLAGS@ @MPI_CFLAGS@ @PHOTON_CFLAGS@
libhpx_la_CFLAGS        += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
libhpx_la_CFLAGS        += -std=c99

libhpx_la_LIBADD         = @MPI_LIBS@ @PHOTON_LIBS@ @LIBCRYPTO_LIBS@
libhpx_la_LIBADD        += @HWLOC_LIBS@ @PAPI_LIBS@ @TCMALLOC_LIBS@
libhpx_la_LIBADD        += @PLATFORM_LIBS@ 

PKGCONFIG_REQUIRES       = @TCMALLOC_PKG@
PKGCONFIG_REQUIRES_PRV   = @MPI_PKG@ @LIBCRYPTO_PKG@ @HWLOC_PKG@ @PAPI_PKG@
PKGCONFIG_CONFLICTS      =
PKGCONFIG_CFLAGS         = $(NETWORK_DEFS) $(MPI_DEFS) $(PHOTON_DEFS)
PKGCONFIG_LIBS           =
PKGCONFIG_LIBS_PRV       = @PHOTON_LIBS@ @PLATFORM_LIBS@

libhpx.pc: $(srcdir)/libhpx.pc.in
	sed -e 's![@]prefix[@]!$(prefix)!g' \
		-e 's![@]exec_prefix[@]!$(exec_prefix)!g' \
		-e 's![@]includedir[@]!$(includedir)/include!g' \
		-e 's![@]libdir[@]!$(libdir)/src!g' \
		-e 's![@]PACKAGE_VERSION[@]!$(PACKAGE_VERSION)!g' \
		-e 's![@]PKGCONFIG_REQUIRES[@]!$(PKGCONFIG_REQUIRES)!g' \
		-e 's![@]PKGCONFIG_REQUIRES_PRIVATE[@]!$(PKGCONFIG_REQUIRES_PRV)!g' \
		-e 's![@]PKGCONFIG_CONFLICTS[@]!$(PKGCONFIG_CONFLICTS)!g' \
		-e 's![@]PKGCONFIG_CFLAGS[@]!$(PKGCONFIG_CFLAGS)!g' \
		-e 's![@]PKGCONFIG_LIBS[@]!$(PKGCONFIG_LIBS)!g' \
		-e 's![@]PKGCONFIG_LIBS_PRIVATE[@]!$(PKGCONFIG_LIBS_PRV)!g' \
		$(srcdir)/libhpx.pc.in > $@

CLEANFILES = libhpx.pc
