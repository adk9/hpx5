#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module load openmpi/1.8.4_thread
    export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/openmpi/1.8.4_thread/include/
}

function add_photon() {
    # This is currently cutter-specific and needs to be generalized.
    export HPX_USE_IB_DEV=$HPXIBDEV
    export HPX_USE_IB_PORT=1
    export HPX_USE_CMA=0
    export HPX_USE_ETH_DEV=roce0
    export HPX_USE_BACKEND=verbs

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

case "$HPXIBDEV" in
    qib0)
        export PSM_MEMORY=large
	CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca btl_openib_if_include $HPXIBDEV\""
	;;
    mlx4_0)
	CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca mtl ^psm --mca btl_openib_if_include $HPXIBDEV\""
	;;
    none)
        ;;
    *)
	;;
esac

case "$HPXCC" in
    gcc-4.6.4)
        CFGFLAGS+=" CC=gcc"
	;;
    gcc-4.9.2)
        module load gcc/4.9.2
        CFGFLAGS+=" CC=gcc"
	;;
    clang)
        CFGFLAGS+=" CC=clang CFLAGS=-Wno-gnu-zero-variadic-macro-arguments "
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
echo ../configure --prefix=${DIR}/build/install/ $CFGFLAGS --enable-testsuite $HPXDEBUG | sh

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
