examplesdir 		= $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS	= pingpong
pingpong_SOURCES	= pingpong.c
pingpong_LDADD		= $(top_builddir)/src/libhpx.la

AM_CPPFLAGS			= -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L

AM_CFLAGS			= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
AM_CFLAGS          += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS          += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS          += @GLIB_CFLAGS@
AM_CFLAGS          += -std=c99
AM_CFLAGS          += -Wno-unused -Wno-unused-but-set-variable # for now
