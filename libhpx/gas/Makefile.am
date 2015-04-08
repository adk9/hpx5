# LT Utility libraries
noinst_LTLIBRARIES	= libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include
libgas_la_SOURCES  = hpx_gas_glue.c \
                     gas.c \
                     gpa.c \
                     smp/smp.c \
                     parcel/memcpy.c

if HAVE_NETWORK
libgas_la_SOURCES += mallctl.c \
                     pgas/actions.c \
                     pgas/bitmap.c \
                     pgas/pgas.c \
                     pgas/heap.c
endif

if HAVE_HUGETLBFS
libgas_la_CFLAGS  = $(HUGETLBFS_CFLAGS)
libgas_la_LIBADD  = $(HUGETLBFS_LIBS)
endif
