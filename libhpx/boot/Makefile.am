# LT Utility libraries
noinst_LTLIBRARIES	  = libboot.la

# libboot files and flags
libboot_la_CPPFLAGS = -I$(top_srcdir)/include
libboot_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libboot_la_SOURCES  = boot.c smp.c 
libboot_la_LIBADD   =

if HAVE_MPI
libboot_la_CFLAGS  += @MPI_CFLAGS@
libboot_la_SOURCES += mpirun.c
libboot_la_LIBADD  += @MPI_LIBS@
endif

if HAVE_PMI
libboot_la_CFLAGS  += @PMI_CFLAGS@
libboot_la_SOURCES += pmi.c
libboot_la_LIBADD  += @PMI_LIBS@
endif
