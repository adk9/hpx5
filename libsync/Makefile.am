####################
# Makefile for HPX #
####################

noinst_LTLIBRARIES       = libsync.la

#network
libsync_la_SOURCES       = locks.c

if CPU_X86_64
libsync_la_SOURCES		+= arch/x86_64/nop.c
endif

libsync_la_CPPFLAGS      = -I$(top_srcdir)/include/sync

libsync_la_CFLAGS        = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
libsync_la_CFLAGS       += -std=c99 
