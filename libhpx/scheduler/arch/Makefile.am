# LT Utility libraries
noinst_LTLIBRARIES = libarch.la

if CPU_X86_64
libarch_la_SOURCES = x86_64/transfer.S x86_64/asm.S
libarch_la_CFLAGS  = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
endif
