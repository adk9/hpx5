# LT Utility libraries
noinst_LTLIBRARIES	= libgas.la

# libgas files and flags
libgas_la_CPPFLAGS = -I$(top_srcdir)/include
libgas_la_SOURCES  = addr.c btt.c \
                     hpx_gas_glue.c \
                     gas.c bitmap.c mallctl.c malloc.c \
                     smp/smp.c \
                     smp/local_only_btt.c \
                     agas_btt.c agas_switch_btt.c \
                     pgas/actions.c \
                     pgas/gva.c \
                     pgas/malloc.c \
                     pgas/pgas.c \
                     pgas/heap.c \
                     pgas/btt.c \
                     parcel/memget.c \
                     parcel/memput.c \
                     parcel/memcpy.c 
