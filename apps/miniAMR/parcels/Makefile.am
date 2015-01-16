noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = miniamr

miniamr_SOURCES  = comm_parent.c \
                   comm_parent_proc.c \
                   comm_parent_reverse.c \
                   comm_proc.c \
                   comm_refine.c \
                   comm_reverse_refine.c \
                   init.c \
                   main.c \
                   move.c \
                   move_blocks.c \
                   plot.c \
                   profile.c \
                   rcb.c \
                   redistribute_blocks.c \
                   refine.c \
                   refine_level.c

miniamr_LDFLAGS  = 
miniamr_LDADD    = $(HPX_LDADD) -lm
miniamr_DEPENDENCIES = $(HPX_LDADD)

miniamr_CPPFLAGS = -I$(top_srcdir)/include
miniamr_CFLAGS   = @MPI_CFLAGS@
miniamr_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

miniamr_CFLAGS  += -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable
miniamr_CFLAGS  += -Wno-uninitialized
