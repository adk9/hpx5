#!/bin/bash

DIR=$1
shift

echo "Building HPX in $DIR"
cd $DIR
set -xe

rm -rf ./build/
./bootstrap
mkdir build
cd build
../configure --with-mpi --enable-photon --with-check --enable-testsuite --enable-apps
make

# TODO: activate tests:
make check
