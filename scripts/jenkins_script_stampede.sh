#!/bin/bash

DIR=$1
shift

function add_init() {
    module load intel/14.0.1.106
    export PSM_MEMORY=large
    export LDFLAGS="-L/opt/ofed/lib64 -lpthread"
    export CPPFLAGS="-I/opt/ofed/include"
}

function add_mpi() {
    # most recent mvapich gets aut-loaded by init() above
}

function add_photon() {
    export HPX_PHOTON_IBDEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=verbs
    export HPX_NETWORK=pwc
}

set -xe

case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi --enable-photon --with-tests-cmd=\"ibrun -n 2 -o 0\""
	add_init
	add_mpi
        add_photon
	;;
    mpi)
	CFGFLAGS=" --with-mpi --with-tests-cmd=\"ibrun -n 2 -o 0\""
	add_init
	add_mpi
	;;
    *)
	CFGFLAGS=" --with-tests-cmd=\"aprun -n 1\""
	add_init
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
echo ../configure --prefix=$DIR/build/HPX5/ CC=mpicc $CFGFLAGS --enable-testsuite $HPXDEBUG | sh

echo "Building HPX."
make
make install

# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep "FAIL:" $DIR/build/tests/unit/test-suite.log
then
    cat $DIR/build/tests/unit/test-suite.log
    exit 1
fi

exit 0
