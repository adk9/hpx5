#!/bin/bash

echo "Building HPX"

rm -rf ./build/
./bootstrap
mkdir build
cd build
../configure --with-mpi --enable-photon --with-check --enable-testsuite --enable-apps
make

# TODO: activate tests:
make check
