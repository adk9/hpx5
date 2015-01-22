#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module load openmpi/1.6.3
    export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/openmpi/1.6.3/include/
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
	CFGFLAGS=" --with-mpi=ompi --enable-photon "
	add_mpi
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

rm -rf ./build/
./bootstrap
mkdir build
cd build
../configure $CFGFLAGS --enable-apps $HPXDEBUG
make

# Run the apps and check their output...
set -xe
case "$HPXMODE" in
    photon)
        cd $DIR/build/apps/lulesh/newfutures
        mpirun -np 16 luleshnewfutures -n 216 -x 48 -i 100
        cd $DIR/build/apps/lulesh/parcels
        mpirun -np 16 luleshparcels -n 216 -x 48 -i 100
        ;;
    mpi)
        cd $DIR/apps/lulesh/mpi
        mpicxx luleshMPI.cc -O3 -o luleshMPI
        mpirun -np 216 luleshMPI 48 100
        cd $DIR/build/apps/lulesh/parcels
        mpirun -np 16 luleshparcels -n 216 -x 48 -i 100
        ;;
esac

exit 0
