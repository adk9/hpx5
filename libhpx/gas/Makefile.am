SUBDIRS =

# LT Utility libraries
noinst_LTLIBRARIES = libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include
libgas_la_SOURCES  = hpx_gas_glue.c \
                     gas.c \
                     gpa.c \
                     smp/smp.c \
                     parcel/memcpy.c
libgas_la_LIBADD   = 

if HAVE_HUGETLBFS
libgas_la_CFLAGS   = $(HUGETLBFS_CFLAGS)
libgas_la_LIBADD  += $(HUGETLBFS_LIBS)
endif

if HAVE_NETWORK
SUBDIRS           += pgas
libgas_la_LIBADD  += pgas/libpgas.la
endif

if BUILD_AGAS
SUBDIRS           += agas
libgas_la_LIBADD  += agas/libagas.la
endif
