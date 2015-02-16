#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module unload PrgEnv-cray 
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
}

function add_photon() {
    # This is currently cutter-specific and needs to be generalized.
    export HPX_USE_IB_DEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=ugni
    export HPX_NETWORK=pwc
}

set -xe

export PSM_MEMORY=large
case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi --enable-photon HPX_PHOTON_CARGS=\"--with-ugni --with-mpi\" --with-hugetlbfs --with-tests-cmd=\"aprun -n 2\""
	add_mpi
        add_photon
	;;
    mpi)
	CFGFLAGS=" --with-mpi --with-tests-cmd=\"aprun -n 2\""
	add_mpi	
	;;
    *)
	CFGFLAGS=" --with-tests-cmd=\"aprun -n 1\""
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
echo ../configure --prefix=$DIR/build/HPX5/ CC=cc $CFGFLAGS --enable-testsuite $HPXDEBUG | sh

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
