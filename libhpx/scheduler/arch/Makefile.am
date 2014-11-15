# LT Utility libraries
noinst_LTLIBRARIES = libarch.la

libarch_la_CPPFLAGS = -I$(top_srcdir)/include

if CPU_X86_64
libarch_la_SOURCES = x86_64/transfer.S x86_64/asm.S x86_64/frame.c
endif
