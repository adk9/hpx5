noinst_appsdir         = $(abs_top_builddir)/apps/

noinst_apps_PROGRAMS   = miniamr

miniamr_SOURCES  = main.c init.c plot.c profile.c refine.c

miniamr_LDFLAGS  = -static
miniamr_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

miniamr_CPPFLAGS = -I$(top_srcdir)/include
miniamr_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
miniamr_CFLAGS  += @PHOTON_CFLAGS@ @MPI_CFLAGS@
miniamr_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

if HAVE_PHOTON
miniamr_LDADD   += -lphoton -ljemalloc
endif
