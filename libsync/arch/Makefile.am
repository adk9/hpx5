noinst_LTLIBRARIES  = libarch.la

libarch_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libsync

if CPU_X86_64
libarch_la_SOURCES  = x86_64/nop.S x86_64/cptr.S
endif

if CPU_ARM
libarch_la_SOURCES = arm/nop.c arm/cptr.S
endif
