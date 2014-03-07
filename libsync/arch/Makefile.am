noinst_LTLIBRARIES  = libarch.la

libarch_la_CPPFLAGS = -I$(top_srcdir)/libsync
libarch_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99 

if CPU_X86_64
libarch_la_SOURCES  = x86_64/nop.c x86_64/cptr.S
endif
