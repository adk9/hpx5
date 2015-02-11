#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module switch PrgEnv-cray PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
}

function add_photon() {
    # This is currently cutter-specific and needs to be generalized.
    export HPX_USE_IB_DEV=mlx4_0
    export HPX_USE_IB_PORT=1
    export HPX_USE_CMA=0
    export HPX_USE_ETH_DEV=roce0
    export HPX_USE_BACKEND=ugni
}

set -xe
export PSM_MEMORY=large
case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi --enable-photon --with-ugni --with-hugetlbfs"
	add_mpi
        add_photon
	;;
    mpi)
	CFGFLAGS=" --with-mpi "
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
../configure --prefix=$DIR/build/HPX5/ CC=cc $CFGFLAGS --enable-testsuite --with-tests-cmd="aprun -n 2 -N 2" $HPXDEBUG
make
make install

# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep -q Failed tests/unit/output.log
then
    exit 1
fi

exit 0
