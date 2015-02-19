#!/bin/bash

OP=$1
DIR=$2
shift

function add_mpi() {
    module load openmpi/1.8.4_thread
}

function add_photon() {
    export HPX_PHOTON_IBDEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=verbs
}

function do_build() {
    echo "Building HPX in $DIR"
    cd $DIR
    
    echo "Bootstrapping HPX."
    #./bootstrap
    
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
    eval $CFG_CMD
    
    echo "Building HPX."
    make -j 8
    make install
}

set -xe

case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi=ompi --enable-photon"
	add_mpi
        add_photon
	;;
    mpi)
	CFGFLAGS=" --with-mpi=ompi"
	add_mpi	
	;;
    *)
	CFGFLAGS=" "
	;;
esac

case "$HPXIBDEV" in
    qib0)
        export PSM_MEMORY=large
	CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca btl_openib_if_include ${HPXIBDEV}\""
	;;
    mlx4_0)
	CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca mtl ^psm --mca btl_openib_if_include ${HPXIBDEV}\""
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

if [ "$OP" == "build" ]; then
    CFG_CMD="../configure --prefix=${DIR}/build/install/ --enable-testsuite --enable-parallel-config ${CFGFLAGS} ${HPXDEBUG}"
    do_build
fi

if [ "$OP" == "run" ]; then
    cd $DIR/build
    # Run all the unit tests:
    make check
fi

exit 0
