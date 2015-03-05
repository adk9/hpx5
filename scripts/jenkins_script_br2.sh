#!/bin/bash -x

OP=$1
DIR=$2
shift

function add_init() {
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/N/home/h/p/hpx5/BigRed2/new_modules/bin:$PATH
}

function add_mpi() {
    add_init
    module load cray-mpich
}

function add_photon() {
    add_init
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
}

function do_build() {
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
    eval $CFG_CMD
    
    echo "Building HPX."
    make -j 8
    make install
}

CFGFLAGS=" --enable-testsuite --enable-parallel-config"
    
case "$HPXMODE" in
    photon)
	CFGFLAGS+=" --with-pmi --with-hugetlbfs --enable-photon --with-tests-cmd=\"aprun -n 2 -N 1\""
        add_photon
	;;
    mpi)
	CFGFLAGS+=" --with-mpi=cray-mpich --with-tests-cmd=\"aprun -n 2 -N 1\""
	add_mpi	
	;;
    *)
	add_init
	;;
esac

case "$HPXCC" in
    *)
	CFGFLAGS+=" CC=cc"
	;;
esac

if [ "$OP" == "build" ]; then
    CFG_CMD="../configure --prefix=${DIR}/build/install/ ${CFGFLAGS} ${HPXDEBUG}"
    do_build
fi

if [ "$OP" == "run" ]; then
    cd $DIR/build
    # Run all the unit tests:
    make check -C tests

    # Check the output of the unit tests:
    if egrep -q "(FAIL:|ERROR:)\s+[1-9][0-9]*" $DIR/build/tests/unit/test-suite.log
    then
	cat $DIR/build/tests/unit/test-suite.log
	exit 1
    fi
fi

exit 0
