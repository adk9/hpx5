#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module load openmpi/1.8.1
    export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/openmpi/1.8.1/include/
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
../configure $CFGFLAGS --with-check --enable-testsuite --enable-apps $HPXDEBUG
make

# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep -q Failed tests/unit/output.log
then
    exit 1
fi

exit 0
