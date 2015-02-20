#!/bin/bash -x

OP=$1
DIR=$2
shift

function add_init() {
    module load intel/14.0.1.106
    export PSM_MEMORY=large
    export LDFLAGS="-L/opt/ofed/lib64 -lpthread"
    export CPPFLAGS="-I/opt/ofed/include"
}

function add_mpi() {
    add_init
    # most recent mvapich gets loaded with intel above
}

function add_photon() {
    add_init
    export HPX_PHOTON_IBDEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=verbs
    export HPX_NETWORK=pwc
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
	CFGFLAGS+=" --with-mpi --enable-photon --with-tests-cmd=\"ibrun -n 2 -o 0\""
        add_photon
	;;
    mpi)
	CFGFLAGS+=" --with-mpi --with-tests-cmd=\"ibrun -n 2 -o 0\""
	add_mpi	
	;;
    *)
	;;
esac

case "$HPXCC" in
    *)
	CFGCFLAGS+=" CC=cc"
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
    if egrep -q "(FAIL:|XFAIL:|ERROR:)\s+[1-9][0-9]*" $DIR/build/tests/unit/test-suite.log
    then
	cat $DIR/build/tests/unit/test-suite.log
	exit 1
    fi
fi

exit 0
