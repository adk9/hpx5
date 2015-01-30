#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module load /home/zalewski/tools/modules/openmpi/1.8.3
}

function add_photon() {
    # This is currently cutter-specific and needs to be generalized.
    export HPX_USE_IB_DEV=mlx4_0
    export HPX_USE_IB_PORT=1
    export HPX_USE_CMA=0
    export HPX_USE_ETH_DEV=roce0

    export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/usr/lib64:$LIBRARY_PATH
}

set -xe
case "$HPXMODE" in
    photon)
	CFGFLAGS=" --enable-photon "
        add_photon
	;;
    mpi)
	CFGFLAGS=" --with-mpi=ompi "
	add_mpi	
	;;
    *)
	CFGFLAGS=" "
	;;
esac

echo "Building HPX in $DIR"
cd $DIR

module load /home/zalewski/tools/modules/gcc/4.9.2
rm -rf ./build/
./bootstrap
mkdir build
cd build
../configure $CFGFLAGS --enable-pedantic --enable-wall --enable-apps --enable-jemalloc --disable-logging CFLAGS="-O3" $HPXDEBUG
make

# Run the apps and check their output...
set -xe
case "$HPXMODE" in
    photon)
        cd $DIR/build/apps/libPXGL/examples        
        # Delta-Stepping
        mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -c -z 40000 --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 25)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.22.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.22.0.ss
        # Chaotic
        mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -c --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 15)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.ss < /dev/null 2>&1 | less
        # Distributed control
        mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -d --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 15)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.ss
        ;;
    mpi)
        cd $DIR/build/apps/libPXGL/examples
        # Delta-Stepping
	mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -c -z 40000 --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 25)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.22.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.22.0.ss
        # Chaotic
        mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -c --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 15)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.ss < /dev/null 2>&1 | less
        # Distributed control
        mpirun -n 10 --map-by node:PE=16 --tag-output sssp -q 60 -d --hpx-cores=16 --hpx-heap=$((1024 * 1024 * 1024 * 15)) --hpx-sendlimit=32 --hpx-recvlimit=10000 /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.gr /u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n/Random4-n.20.0.ss
        ;;
esac

exit 0
