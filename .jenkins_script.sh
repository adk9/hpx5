#!/bin/bash

DIR=$1
shift

function add_mpi() {
    # This is currently cutter-specific and needs to be generalized.
    module load openmpi/1.6.3
    export C_INCLUDE_PATH=$C_INCLUDE_PATH:/opt/openmpi/1.6.3/include/
}

set -xe
case "$HPXMODE" in
    photon)
	CFGFLAGS=" --with-mpi --enable-photon "
	add_mpi	
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
../configure $CFGFLAGS --with-check --enable-testsuite --enable-apps $HPXDEBUG
make

# Run all the unit tests:
#make check

# Check the output of the unit tests:
if [ $(grep -c Failed tests/unit/output.log) -ne 0 ]
then
    exit 1
fi

# TODO: Run the apps and check their output...

exit 0
