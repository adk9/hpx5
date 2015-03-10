# The isend-irecv network implementations
noinst_LTLIBRARIES	= libisir.la

libisir_la_CPPFLAGS = -I$(top_srcdir)/include
libisir_la_CFLAGS   = $(MPI_CFLAGS)
libisir_la_SOURCES  = isend_buffer.c irecv_buffer.c funneled.c emulate_pwc.c
libisir_la_LIBADD   = $(MPI_LIBS)

if HAVE_APEX
libisir_la_CPPFLAGS += @APEX_CFLAGS@
libisir_la_LIBADD   += @APEX_LIBS@
endif
