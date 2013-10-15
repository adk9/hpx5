examplesdir 		= $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS	= pingpong
pingpong_SOURCES	= pingpong.c
pingpong_LDADD		= $(top_builddir)/src/libhpx.la

if HAVE_NETWORK
    NETWORK_DEFS	= -DHAVE_NETWORK=1
endif

if HAVE_MPI
    MPI_DEFS		= -DHAVE_MPI=1
endif

if HAVE_PHOTON
    PHOTON_DEFS		= -DHAVE_PHOTON=1
endif

AM_CPPFLAGS			= -I$(top_srcdir)/include
AM_CPPFLAGS        += $(NETWORK_DEFS) $(MPI_DEFS) $(PHOTON_DEFS) 

AM_CFLAGS			= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
AM_CFLAGS 		   += $(HPX_F_LTO)
AM_CFLAGS          += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @LIBCRYPTO_CFLAGS@
AM_CFLAGS          += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS          += -std=c99
AM_CFLAGS          += -Wno-unused -Wno-unused-but-set-variable # for now
