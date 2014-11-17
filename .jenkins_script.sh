#!/bin/bash

echo "Building HPX"

rm -rf ./build/
./bootstrap
mkdir build
cd build
../configure 
make

# TODO: activate tests:
make check
