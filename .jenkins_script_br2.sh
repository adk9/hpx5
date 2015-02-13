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
    export HPX_PHOTON_CARGS="--with-ugni"
    export HPX_USE_IB_DEV=$HPXIBDEV
}

set -xe

export PSM_MEMORY=large
case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi --enable-photon --with-hugetlbfs"
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

case "$HPXIBDEV" in
    mlx4_0)
        CFGFLAGS+=" --with-tests-cmd=\"aprun -n 2 --mca mtl ^psm --mca btl_openib_if_include $HPXIBDEV\""
        ;;
    none)
        ;;
    *)
        ;;
esac

echo "Building HPX in $DIR"
cd $DIR

echo "Bootstrapping HPX."
./bootstrap

if [ -d "./build" ]; then
        rm -rf ./build/
fi
mkdir build
cd build

if [ -d "./install" ]; then
        rm -rf ./install/
fi
mkdir install

echo "Configuring HPX."
../configure --prefix=$DIR/build/HPX5/ CC=cc $CFGFLAGS --enable-testsuite $HPXDEBUG

echo "Building HPX."
make
make install

# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep "FAIL:" tests/unit/test-suite.log
then
    cat tests/unit/test-suite.log
    exit 1
fi

exit 0
