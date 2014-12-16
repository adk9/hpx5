# LT Utility libraries
noinst_LTLIBRARIES	= libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include -DJEMALLOC_RENAME=libhpx_global_ -DJEMALLOC_NAMESPACE=hpx
libgas_la_SOURCES  = hpx_gas_glue.c \
                     gas.c \
                     mallctl.c \
                     smp/smp.c \
                     pgas/actions.c \
                     pgas/bitmap.c \
                     pgas/gpa.c \
                     pgas/pgas.c \
                     pgas/heap.c \
                     parcel/memget.c \
                     parcel/memput.c \
                     parcel/memcpy.c 
