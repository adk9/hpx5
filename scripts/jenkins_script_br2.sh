#!/bin/bash

module load craype-hugepages8M

DIR=$1
shift

set -xe
cd $DIR

cd build
# Run all the unit tests:
make check

# Check the output of the unit tests:
if grep "FAIL:" $DIR/build/tests/unit/test-suite.log
then
    cat $DIR/build/tests/unit/test-suite.log
    exit 1
fi

exit 0
