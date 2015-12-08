#!/bin/sh

prefix=/u/jsfiroz/hpx_2_pre/hpx_simple_coalescing/hpx/install
exec_prefix=/u/jsfiroz/hpx_2_pre/hpx_simple_coalescing/hpx/install
libdir=${exec_prefix}/lib

LD_PRELOAD=${libdir}/libjemalloc.so.2
export LD_PRELOAD
exec "$@"
