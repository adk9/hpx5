#!/bin/bash

../configure --prefix=/u/jsfiroz/hpx_2_pre/hpx_simple_coalescing/hpx/install --enable-parallel-config  --enable-mpi --enable-photon  CFLAGS='-O0 -g' --enable-debug


#--enable-testsuite    --enable-static --disable-shared --enable-lengthy-tests

#../configure --prefix=/u/jsfiroz/hpx_2_pre/hpx_simple_coalescing/hpx/install --enable-parallel-config  --enable-mpi --enable-photon --enable-debug --enable-instrumentation --enable-lengthy-tests  CFLAGS='-O0 -g' --enable-testsuite    --disable-static --enable-shared

# -fsanitize=address

#PKG_CONFIG_PATH=/opt/openmpi/1.8.4_thread/lib/pkgconfig

#--enable-jemalloc CC=gcc --enable-logging

#--with-mpi=ompi  --enable-profiling  -O3 --enable-static --enable-shared


#../configure --prefix=/nfs/home/hpx5/jenkins_workspaces/workspace/HPX_Cutter_Regression_tests/BUILD_AXIS/shared/HPXCC/gcc-5.2.0/HPXMODE_AXIS/mpi+pgas+je/OPTIMIZATION/opt/SYSTEM/cutter_mlx4_0/hpx/build/install/  --disable-static --enable-shared CC=gcc CFLAGS="-O3" --enable-mpi --enable-lengthy-tests --enable-parallel-config
